/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/bartonsun/D2ModdingToolset)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "lobbysaveexchange.h"
#include "dynamiccast.h"
#include "gamesettings.h"
#include "midclient.h"
#include "midgard.h"
#include "netcustomsession.h"
#include "phase.h"
#include "phasegame.h"
#include "utils.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <slikenet/BitStream.h>
#include <spdlog/spdlog.h>
#include <string>
#include <thread>
#include <vector>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern std::thread::id mainThreadId;

namespace hooks {
namespace {

using namespace LobbyProtocol;

static constexpr auto storedAckGrace{std::chrono::seconds(10)};
static constexpr unsigned int collisionSuffixLimit{9999};

struct StableFileIdentity
{
    DWORD volumeSerialNumber{};
    DWORD fileIndexHigh{};
    DWORD fileIndexLow{};
};

class NativeFileHandle
{
public:
    NativeFileHandle() = default;
    explicit NativeFileHandle(HANDLE handle)
        : handle{handle}
    { }
    ~NativeFileHandle()
    {
        reset();
    }

    NativeFileHandle(const NativeFileHandle&) = delete;
    NativeFileHandle& operator=(const NativeFileHandle&) = delete;
    NativeFileHandle(NativeFileHandle&& other) noexcept
        : handle{other.handle}
    {
        other.handle = INVALID_HANDLE_VALUE;
    }
    NativeFileHandle& operator=(NativeFileHandle&& other) noexcept
    {
        if (this != &other) {
            reset();
            handle = other.handle;
            other.handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    void reset(HANDLE newHandle = INVALID_HANDLE_VALUE)
    {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
        handle = newHandle;
    }

    HANDLE get() const
    {
        return handle;
    }

private:
    HANDLE handle{INVALID_HANDLE_VALUE};
};

struct SaveTransferSession
{
    SaveRequestV3 request;
    std::string saveName;
    std::filesystem::path savePath;
    StableFileIdentity reservedFileIdentity;
    NativeFileHandle pathClaim;
    NativeFileHandle snapshotFile;
    std::chrono::steady_clock::time_point deadline;
};

struct PendingStoredAck
{
    std::uint64_t saveId{};
    std::filesystem::path savePath;
    NativeFileHandle snapshotFile;
    std::chrono::steady_clock::time_point deadline;
};

std::optional<SaveTransferSession> activeTransfer;
std::optional<PendingStoredAck> pendingStoredAck;
/** A native save request cannot be cancelled. Keep every path handed to it reserved for this DLL
 * lifetime so a late GameSaved callback can never match a later request after timeout/teardown. */
std::vector<std::filesystem::path> nativeSavePathTombstones;

std::optional<StableFileIdentity> readStableFileIdentity(HANDLE handle);
bool sameFileObjectIdentity(const StableFileIdentity& first, const StableFileIdentity& second);
bool deleteEmptyFileWithMatchingObjectIdentity(const std::filesystem::path& path,
                                               const StableFileIdentity& expected);

bool markOpenFileForDeletion(HANDLE handle)
{
    FILE_DISPOSITION_INFO disposition{};
    disposition.DeleteFile = TRUE;
    return handle != INVALID_HANDLE_VALUE
           && SetFileInformationByHandle(handle, FileDispositionInfo, &disposition,
                                         sizeof(disposition));
}

bool isMainThread()
{
    return std::this_thread::get_id() == mainThreadId;
}

bool isDeadlineExpired(const SaveTransferSession& transfer)
{
    return std::chrono::steady_clock::now() >= transfer.deadline;
}

bool isLocalOnly(const SaveRequestV3& request)
{
    return request.wireVersion == saveRequestVersionV3
           && request.mode == SaveModeV3::LocalOnly;
}

std::filesystem::path getSaveFolder(const game::CMidgard* midgard)
{
    std::filesystem::path folder{gameFolder()};

    auto settings = midgard->data->settings;
    if (settings && *settings && (*settings)->saveGameFolder.string) {
        folder /= (*settings)->saveGameFolder.string;
    } else {
        folder /= "SaveGame";
    }

    return folder;
}

bool sameWindowsPath(const std::filesystem::path& first,
                     const std::filesystem::path& second)
{
    try {
        auto firstNormalized{first.lexically_normal()};
        auto secondNormalized{second.lexically_normal()};
        firstNormalized.make_preferred();
        secondNormalized.make_preferred();
        const auto firstNative{firstNormalized.native()};
        const auto secondNative{secondNormalized.native()};
        if (firstNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
            || secondNative.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            return false;
        }

        return CompareStringOrdinal(firstNative.data(), static_cast<int>(firstNative.size()),
                                    secondNative.data(), static_cast<int>(secondNative.size()), TRUE)
            == CSTR_EQUAL;
    } catch (...) {
        return false;
    }
}

bool nativeSavePathIsReserved(const std::filesystem::path& path)
{
    return std::any_of(nativeSavePathTombstones.begin(), nativeSavePathTombstones.end(),
                       [&](const auto& reserved) { return sameWindowsPath(path, reserved); });
}

void reserveNativeSavePath(std::filesystem::path path)
{
    path = path.lexically_normal();
    path.make_preferred();
    nativeSavePathTombstones.push_back(std::move(path));
}

std::string makeLegacySaveName(std::uint64_t saveId)
{
    std::ostringstream result;
    result << "LobbyMatchHost_" << std::hex << std::setw(16) << std::setfill('0') << saveId;
    return result.str();
}

bool chooseUnusedSavePath(const game::CMidgard* midgard, SaveTransferSession& transfer)
{
    const auto baseName{transfer.request.saveStem.empty()
                            ? makeLegacySaveName(transfer.request.saveId)
                            : transfer.request.saveStem};
    const auto folder{getSaveFolder(midgard)};

    for (unsigned int index = 1; index <= collisionSuffixLimit; ++index) {
        const auto suffix{index == 1 ? std::string{} : '_' + std::to_string(index)};
        const auto saveName{baseName + suffix};
        const auto fileName{saveName + ".sg"};
        if (fileName.size() > saveResultFileNameMax) {
            return false;
        }

        const auto savePath{folder / fileName};
        if (nativeSavePathIsReserved(savePath)) {
            continue;
        }

        // Claim the path atomically while sharing read/write access so the native save can populate
        // the reserved object. Its object identity is checked again before any bytes are uploaded.
        const HANDLE placeholder{CreateFileW(savePath.c_str(), 0,
                                             FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr)};
        if (placeholder == INVALID_HANDLE_VALUE) {
            const auto createError{GetLastError()};
            if (createError == ERROR_FILE_EXISTS || createError == ERROR_ALREADY_EXISTS
                || GetFileAttributesW(savePath.c_str()) != INVALID_FILE_ATTRIBUTES) {
                continue;
            }
            spdlog::warn(__FUNCTION__ ": cannot reserve save path '{:s}', error {:d}",
                         savePath.string(), static_cast<int>(createError));
            return false;
        }

        const auto identity{readStableFileIdentity(placeholder)};
        if (!identity) {
            CloseHandle(placeholder);
            reserveNativeSavePath(savePath);
            spdlog::warn(__FUNCTION__
                         ": cannot identify reserved save path '{:s}'; leaving reserved file",
                         savePath.string());
            return false;
        }

        try {
            reserveNativeSavePath(savePath);
            transfer.saveName = saveName;
            transfer.savePath = savePath;
            transfer.reservedFileIdentity = *identity;
            transfer.pathClaim.reset(placeholder);
        } catch (...) {
            CloseHandle(placeholder);
            deleteEmptyFileWithMatchingObjectIdentity(savePath, *identity);
            spdlog::warn(__FUNCTION__ ": could not retain reserved save path state");
            return false;
        }

        return true;
    }

    return false;
}

bool sendSaveDataPrefix(SLNet::BitStream& stream,
                        std::uint64_t saveId,
                        SaveDataOperationV2 operation)
{
    auto service{CNetCustomService::get()};
    if (!service) {
        return false;
    }

    stream.Write(static_cast<SLNet::MessageID>(ID_LOBBY_SAVE_UPLOAD));
    stream.Write(saveTransferVersion);
    stream.Write(saveId);
    stream.Write(static_cast<std::uint8_t>(operation));
    return true;
}

bool sendLobbyPayload(SLNet::BitStream& stream)
{
    auto service{CNetCustomService::get()};
    return service
        && service->send(stream, service->getLobbyGuid(), PacketPriority::MEDIUM_PRIORITY);
}

bool sendNativeSaveResult(std::uint64_t saveId,
                          std::uint8_t resultCode,
                          const std::string& fileName = {})
{
    const bool succeeded{resultCode == nativeSaveResultSuccess};
    if (saveId == 0 || fileName.size() > saveResultFileNameMax
        || succeeded == fileName.empty()) {
        return false;
    }

    SLNet::BitStream stream;
    stream.Write(static_cast<SLNet::MessageID>(ID_LOBBY_SAVE_NATIVE_RESULT));
    stream.Write(saveRequestVersionV3);
    stream.Write(saveId);
    stream.Write(resultCode);
    const auto length{static_cast<std::uint16_t>(fileName.size())};
    stream.Write(length);
    if (length != 0) {
        stream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(fileName.data()), length);
    }
    return sendLobbyPayload(stream);
}

bool seekToFileStart(HANDLE handle)
{
    LARGE_INTEGER start{};
    return handle != INVALID_HANDLE_VALUE && SetFilePointerEx(handle, start, nullptr, FILE_BEGIN);
}

bool validateSaveSignature(HANDLE handle)
{
    static constexpr char signature[]{"D2EESFISIG"};
    std::array<char, sizeof(signature) - 1> actual{};
    DWORD bytesRead{};
    return seekToFileStart(handle)
        && ReadFile(handle, actual.data(), static_cast<DWORD>(actual.size()), &bytesRead, nullptr)
        && bytesRead == actual.size()
        && std::equal(actual.begin(), actual.end(), std::begin(signature));
}

bool captureImmutableSave(SaveTransferSession& transfer,
                          std::uint32_t& totalSize,
                          SaveFailureV2& failure)
{
    // The zero-access placeholder was opened without FILE_SHARE_DELETE, so close it after
    // GameSaved before acquiring a DELETE-capable snapshot handle. The object identity check below
    // fails closed if the path was replaced during that transition.
    transfer.pathClaim.reset();
    const HANDLE handle{CreateFileW(transfer.savePath.c_str(),
                                    GENERIC_READ | FILE_READ_ATTRIBUTES | DELETE,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
    if (handle == INVALID_HANDLE_VALUE) {
        spdlog::warn(__FUNCTION__ ": cannot lock save snapshot '{:s}', error {:d}",
                     transfer.savePath.string(), static_cast<int>(GetLastError()));
        failure = SaveFailureV2::FileIo;
        return false;
    }
    transfer.snapshotFile.reset(handle);

    const auto identity{readStableFileIdentity(transfer.snapshotFile.get())};
    if (!identity) {
        failure = SaveFailureV2::FileIo;
        return false;
    }
    if (!sameFileObjectIdentity(*identity, transfer.reservedFileIdentity)) {
        spdlog::warn(__FUNCTION__ ": native save path identity changed for '{:s}'",
                     transfer.savePath.string());
        failure = SaveFailureV2::CaptureFailed;
        return false;
    }

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(transfer.snapshotFile.get(), &fileSize) || fileSize.QuadPart <= 0) {
        failure = SaveFailureV2::FileIo;
        return false;
    }
    const auto fileSize64{static_cast<std::uint64_t>(fileSize.QuadPart)};
    if (fileSize64 > transfer.request.maxBytes || fileSize64 > saveFileHardLimit
        || fileSize64 > std::numeric_limits<std::uint32_t>::max()) {
        failure = SaveFailureV2::TooLarge;
        return false;
    }
    if (isDeadlineExpired(transfer)) {
        failure = SaveFailureV2::TimedOut;
        return false;
    }
    if (!validateSaveSignature(transfer.snapshotFile.get())) {
        failure = SaveFailureV2::CaptureFailed;
        return false;
    }

    totalSize = static_cast<std::uint32_t>(fileSize64);
    return true;
}

bool uploadSave(SaveTransferSession& transfer,
                std::uint32_t totalSize,
                SaveFailureV2& failure)
{
    if (!seekToFileStart(transfer.snapshotFile.get())) {
        failure = SaveFailureV2::FileIo;
        return false;
    }

    SLNet::BitStream begin;
    if (!sendSaveDataPrefix(begin, transfer.request.saveId, SaveDataOperationV2::Begin)) {
        failure = SaveFailureV2::SendFailed;
        return false;
    }
    begin.Write(totalSize);
    if (!sendLobbyPayload(begin)) {
        failure = SaveFailureV2::SendFailed;
        return false;
    }

    std::array<unsigned char, saveChunkSizeMax> buffer{};
    for (std::uint32_t offset = 0; offset < totalSize;) {
        if (isDeadlineExpired(transfer)) {
            failure = SaveFailureV2::TimedOut;
            return false;
        }

        const auto chunkSize{static_cast<std::uint16_t>(
            std::min<std::uint32_t>(saveChunkSizeMax, totalSize - offset))};
        DWORD bytesRead{};
        if (!ReadFile(transfer.snapshotFile.get(), buffer.data(), chunkSize, &bytesRead, nullptr)
            || bytesRead != chunkSize) {
            failure = SaveFailureV2::FileIo;
            return false;
        }

        SLNet::BitStream chunk;
        if (!sendSaveDataPrefix(chunk, transfer.request.saveId, SaveDataOperationV2::Chunk)) {
            failure = SaveFailureV2::SendFailed;
            return false;
        }
        chunk.Write(offset);
        chunk.Write(chunkSize);
        chunk.WriteAlignedBytes(buffer.data(), chunkSize);
        if (!sendLobbyPayload(chunk)) {
            failure = SaveFailureV2::SendFailed;
            return false;
        }
        offset += chunkSize;
    }

    if (isDeadlineExpired(transfer)) {
        failure = SaveFailureV2::TimedOut;
        return false;
    }

    SLNet::BitStream commit;
    if (!sendSaveDataPrefix(commit, transfer.request.saveId, SaveDataOperationV2::Commit)) {
        failure = SaveFailureV2::SendFailed;
        return false;
    }
    if (!sendLobbyPayload(commit)) {
        failure = SaveFailureV2::SendFailed;
        return false;
    }

    spdlog::info(__FUNCTION__
                 ": uploaded native host save id {:016x}, {:d} bytes; awaiting stored ACK",
                 transfer.request.saveId, totalSize);
    return true;
}

game::CPhaseGame* getHostPhaseGame(const game::CMidgard* midgard)
{
    if (!isMainThread() || !midgard || !midgard->data || !midgard->data->multiplayerGame
        || !midgard->data->host || !midgard->data->gameIsRunning || !midgard->data->client) {
        return nullptr;
    }

    const auto client{midgard->data->client};
    if (!client->data || !client->data->scenarioStarted || !client->data->phase) {
        return nullptr;
    }

    const auto phase{client->data->phase};
    const auto typeIdOperator{game::RttiApi::get().typeIdOperator};
    if (!phase->vftable || !typeIdOperator || !*typeIdOperator) {
        return nullptr;
    }

    // CPhase has multiple concrete owners. Ask the game's own MSVC RTTI helper for the exact
    // dynamic type before deriving the enclosing CPhaseGame object.
    static constexpr char phaseGameTypeName[]{".?AVCPhaseGame@@"};
    const game::TypeDescriptor* phaseType{};
    try {
        phaseType = (*typeIdOperator)(phase);
    } catch (...) {
        return nullptr;
    }
    if (!phaseType || std::strcmp(phaseType->name, phaseGameTypeName) != 0) {
        return nullptr;
    }

    const auto phaseGame{reinterpret_cast<game::CPhaseGame*>(
        reinterpret_cast<std::uint8_t*>(phase) - offsetof(game::CPhaseGame, phase))};
    return phaseGame->data && phaseGame->data->midClient == client ? phaseGame : nullptr;
}

std::optional<StableFileIdentity> readStableFileIdentity(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION information{};
    if (handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(handle, &information)
        || (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return std::nullopt;
    }

    StableFileIdentity identity{};
    identity.volumeSerialNumber = information.dwVolumeSerialNumber;
    identity.fileIndexHigh = information.nFileIndexHigh;
    identity.fileIndexLow = information.nFileIndexLow;
    return identity;
}

bool sameFileObjectIdentity(const StableFileIdentity& first,
                            const StableFileIdentity& second)
{
    return first.volumeSerialNumber == second.volumeSerialNumber
           && first.fileIndexHigh == second.fileIndexHigh
           && first.fileIndexLow == second.fileIndexLow;
}

bool deleteEmptyFileWithMatchingObjectIdentity(const std::filesystem::path& path,
                                               const StableFileIdentity& expected)
{
    const HANDLE handle{CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | DELETE, 0, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    const auto actual{readStableFileIdentity(handle)};
    LARGE_INTEGER fileSize{};
    const bool unchangedPlaceholder{actual && sameFileObjectIdentity(*actual, expected)
                                    && GetFileSizeEx(handle, &fileSize) && fileSize.QuadPart == 0};
    const bool removed{unchangedPlaceholder && markOpenFileForDeletion(handle)};
    CloseHandle(handle);
    return removed;
}

bool isExpectedHostSaveResultPath(const std::string& observed,
                                  const std::filesystem::path& expected)
{
    if (observed.empty()) {
        return false;
    }

    try {
        auto expectedPath{expected};
        if (expectedPath.is_relative()) {
            expectedPath = std::filesystem::path{gameFolder()} / expectedPath;
        }
        expectedPath = expectedPath.lexically_normal();

        std::filesystem::path observedPath{observed};
        if (observedPath.is_relative()) {
            observedPath = observedPath.has_parent_path()
                ? std::filesystem::path{gameFolder()} / observedPath
                : expectedPath.parent_path() / observedPath;
        }
        observedPath = observedPath.lexically_normal();
        return sameWindowsPath(observedPath, expectedPath);
    } catch (...) {
        return false;
    }
}

bool clientIsNativeHost(const CNetCustomSession* session, const game::CMidgard* midgard)
{
    if (!session || !midgard || !midgard->data) {
        return false;
    }

    const bool sessionHost{session->isHost()};
    const bool midgardHost{midgard->data->host};
    if (sessionHost != midgardHost) {
        spdlog::warn(__FUNCTION__ ": refusing save request while host state is inconsistent");
        return false;
    }
    return sessionHost;
}

void awaitStoredAck()
{
    if (!activeTransfer) {
        return;
    }

    PendingStoredAck pending{};
    pending.saveId = activeTransfer->request.saveId;
    pending.savePath = std::move(activeTransfer->savePath);
    pending.snapshotFile = std::move(activeTransfer->snapshotFile);
    pending.deadline = activeTransfer->deadline + storedAckGrace;
    pendingStoredAck.emplace(std::move(pending));
    activeTransfer.reset();
}

} // namespace

void sendLobbySaveFailure(std::uint64_t saveId, LobbyProtocol::SaveFailureV2 failure)
{
    SLNet::BitStream stream;
    if (!sendSaveDataPrefix(stream, saveId, LobbyProtocol::SaveDataOperationV2::Fail)) {
        return;
    }
    stream.Write(static_cast<std::uint8_t>(failure));
    sendLobbyPayload(stream);
}

void sendLobbySaveFailure(const LobbyProtocol::SaveRequestV3& request,
                          LobbyProtocol::SaveFailureV2 failure)
{
    if (isLocalOnly(request)) {
        sendNativeSaveResult(request.saveId, static_cast<std::uint8_t>(failure));
    } else {
        sendLobbySaveFailure(request.saveId, failure);
    }
}

void handleLobbySaveStoredAck(std::uint64_t saveId)
{
    if (!isMainThread() || saveId == 0) {
        spdlog::warn(__FUNCTION__ ": refusing stored ACK outside the main/UI thread or with zero id");
        return;
    }
    if (!pendingStoredAck || pendingStoredAck->saveId != saveId) {
        return;
    }

    if (markOpenFileForDeletion(pendingStoredAck->snapshotFile.get())) {
        spdlog::info(__FUNCTION__ ": removed acknowledged local save '{:s}', id {:016x}",
                     pendingStoredAck->savePath.string(), saveId);
    } else {
        spdlog::warn(__FUNCTION__
                     ": cannot delete acknowledged local save '{:s}', error {:d}; retaining file",
                     pendingStoredAck->savePath.string(), static_cast<int>(GetLastError()));
    }

    pendingStoredAck.reset();
}

void handleLobbySaveRequest(const LobbyProtocol::SaveRequestV3& request)
{
    using namespace LobbyProtocol;

    if (!isMainThread()) {
        spdlog::warn(__FUNCTION__ ": refusing capture outside the main/UI thread");
        sendLobbySaveFailure(request, SaveFailureV2::UnsafePhase);
        return;
    }
    if (!game::CPhaseGameApi::nativeSaveSupported()) {
        sendLobbySaveFailure(request, SaveFailureV2::UnsupportedGameBuild);
        return;
    }
    expireLobbySaveTransfers();
    if (request.role != SaveRoleV2::Host || request.saveId == 0 || request.maxBytes == 0
        || request.maxBytes > saveFileHardLimit || request.timeoutMs == 0
        || (request.wireVersion == saveTransferVersion
            && request.mode != SaveModeV3::Upload)) {
        sendLobbySaveFailure(request,
                             request.role == SaveRoleV2::Joiner ? SaveFailureV2::WrongRole
                                                                : SaveFailureV2::MalformedRequest);
        return;
    }
    if (pendingStoredAck) {
        if (pendingStoredAck->saveId == request.saveId) {
            return;
        }
        sendLobbySaveFailure(request, SaveFailureV2::Busy);
        return;
    }
    if (activeTransfer) {
        if (activeTransfer->request.saveId == request.saveId) {
            return;
        }
        sendLobbySaveFailure(request, SaveFailureV2::Busy);
        return;
    }

    auto service{CNetCustomService::get()};
    auto midgard{game::CMidgardApi::get().instance()};
    auto session{service ? service->getSession() : nullptr};
    if (!service || !session || !midgard || !midgard->data || !midgard->data->multiplayerGame
        || !midgard->data->client) {
        sendLobbySaveFailure(request, SaveFailureV2::NoActiveGame);
        return;
    }
    if (!clientIsNativeHost(session, midgard)) {
        sendLobbySaveFailure(request, SaveFailureV2::WrongRole);
        return;
    }

    const auto sendSaveGameMsg{game::CPhaseGameApi::get().sendSaveGameMsg};
    if (!sendSaveGameMsg) {
        sendLobbySaveFailure(request, SaveFailureV2::UnsupportedGameBuild);
        return;
    }
    const auto phaseGame{getHostPhaseGame(midgard)};
    if (!phaseGame) {
        sendLobbySaveFailure(request, SaveFailureV2::UnsafePhase);
        return;
    }

    SaveTransferSession transfer{};
    transfer.request = request;
    transfer.deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(request.timeoutMs);
    if (!chooseUnusedSavePath(midgard, transfer)) {
        sendLobbySaveFailure(request, SaveFailureV2::FileIo);
        return;
    }

    activeTransfer.emplace(std::move(transfer));
    spdlog::info(__FUNCTION__ ": requesting native host save '{:s}', id {:016x}, mode {:d}",
                 activeTransfer->saveName, request.saveId, static_cast<int>(request.mode));
    try {
        // false is the native non-UI/autosave form. The wrapper's true form owns an additional
        // UI-lock increment which a direct CPhaseGame call must not request.
        sendSaveGameMsg(phaseGame, activeTransfer->saveName.c_str(), false);
    } catch (...) {
        spdlog::warn(__FUNCTION__ ": native host save builder raised a C++ exception");
        const auto placeholderPath{activeTransfer->savePath};
        const auto placeholderIdentity{activeTransfer->reservedFileIdentity};
        activeTransfer.reset();
        if (!deleteEmptyFileWithMatchingObjectIdentity(placeholderPath, placeholderIdentity)) {
            spdlog::warn(__FUNCTION__
                         ": could not remove unchanged native-save placeholder '{:s}'",
                         placeholderPath.string());
        }
        sendLobbySaveFailure(request, SaveFailureV2::CaptureFailed);
    }
}

bool hasActiveLobbyHostSaveTransfer()
{
    return isMainThread() && activeTransfer.has_value();
}

void handleGameSavedForLobby(bool success, const std::string& savePath)
{
    using namespace LobbyProtocol;

    if (!activeTransfer
        || !isExpectedHostSaveResultPath(savePath, activeTransfer->savePath)) {
        return;
    }

    auto service{CNetCustomService::get()};
    auto midgard{game::CMidgardApi::get().instance()};
    auto session{service ? service->getSession() : nullptr};
    SaveFailureV2 failure{SaveFailureV2::CaptureFailed};
    if (!clientIsNativeHost(session, midgard)) {
        failure = SaveFailureV2::WrongRole;
    } else if (isDeadlineExpired(*activeTransfer)) {
        failure = SaveFailureV2::TimedOut;
    } else if (!success) {
        failure = SaveFailureV2::CaptureFailed;
    } else {
        std::uint32_t totalSize{};
        if (captureImmutableSave(*activeTransfer, totalSize, failure)) {
            const auto isV3{activeTransfer->request.wireVersion == saveRequestVersionV3};
            const auto fileName{activeTransfer->savePath.filename().string()};
            if (isV3
                && !sendNativeSaveResult(activeTransfer->request.saveId,
                                         nativeSaveResultSuccess, fileName)) {
                spdlog::warn(__FUNCTION__ ": failed to report native save filename {:016x}",
                             activeTransfer->request.saveId);
                failure = SaveFailureV2::SendFailed;
            } else if (isLocalOnly(activeTransfer->request)) {
                spdlog::info(__FUNCTION__ ": retained local-only save '{:s}', id {:016x}",
                             activeTransfer->savePath.string(),
                             activeTransfer->request.saveId);
                activeTransfer.reset();
                return;
            } else if (uploadSave(*activeTransfer, totalSize, failure)) {
                awaitStoredAck();
                return;
            }
        }
    }

    const auto request{activeTransfer->request};
    activeTransfer.reset();
    sendLobbySaveFailure(request, failure);
}

void expireLobbySaveTransfers()
{
    if (!isMainThread()) {
        return;
    }

    if (activeTransfer && isDeadlineExpired(*activeTransfer)) {
        const auto request{activeTransfer->request};
        spdlog::warn(__FUNCTION__ ": lobby host save {:016x}, mode {:d}, timed out",
                     request.saveId, static_cast<int>(request.mode));
        activeTransfer.reset();
        sendLobbySaveFailure(request, SaveFailureV2::TimedOut);
    }

    if (pendingStoredAck && std::chrono::steady_clock::now() >= pendingStoredAck->deadline) {
        spdlog::warn(__FUNCTION__
                     ": stored ACK for local save {:016x} timed out; retaining file",
                     pendingStoredAck->saveId);
        pendingStoredAck.reset();
    }
}

void terminateLobbySaveTransfers()
{
    if (activeTransfer) {
        const auto request{activeTransfer->request};
        activeTransfer.reset();
        sendLobbySaveFailure(request, SaveFailureV2::TimedOut);
    }

    if (pendingStoredAck) {
        spdlog::info(__FUNCTION__
                     ": forgetting stored ACK wait for {:016x}; retaining local file",
                     pendingStoredAck->saveId);
        pendingStoredAck.reset();
    }
}

void resetLobbySaveTransferState()
{
    activeTransfer.reset();
    pendingStoredAck.reset();
}

} // namespace hooks
