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
#include <limits>
#include <optional>
#include <slikenet/BitStream.h>
#include <spdlog/spdlog.h>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

extern std::thread::id mainThreadId;

namespace hooks {
namespace {

using namespace LobbyProtocol;

static constexpr auto storedAckGrace{std::chrono::seconds(10)};
static constexpr auto saveRequestTimeout{std::chrono::seconds(30)};
static constexpr unsigned int collisionSuffixLimit{9999};

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
    SaveRequest request;
    std::string saveName;
    std::filesystem::path savePath;
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
std::uint64_t nativeSaveRequestSequence{};

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

bool isLocalOnly(const SaveRequest& request)
{
    return request.mode == SaveMode::LocalOnly;
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

bool chooseUnusedSavePath(const game::CMidgard* midgard, SaveTransferSession& transfer)
{
    const auto& baseName{transfer.request.saveStem};
    const auto folder{getSaveFolder(midgard)};
    if (nativeSaveRequestSequence == std::numeric_limits<std::uint64_t>::max()) {
        return false;
    }
    const auto sequence{++nativeSaveRequestSequence};
    const auto requestSuffix{sequence == 1 ? std::string{} : '_' + std::to_string(sequence)};

    for (unsigned int index = 1; index <= collisionSuffixLimit; ++index) {
        const auto collisionSuffix{index == 1 ? std::string{} : '_' + std::to_string(index)};
        const auto suffix{requestSuffix + collisionSuffix};
        if (suffix.size() + 3 >= saveResultFileNameMax) {
            return false;
        }
        const auto maxBaseSize{saveResultFileNameMax - suffix.size() - 3};
        const auto saveName{baseName.substr(0, maxBaseSize) + suffix};
        const auto fileName{saveName + ".sg"};

        const auto savePath{folder / fileName};
        std::error_code error;
        if (std::filesystem::exists(savePath, error)) {
            continue;
        }
        if (error) {
            spdlog::warn(__FUNCTION__ ": cannot inspect save path '{:s}': {:s}",
                         savePath.string(), error.message());
            return false;
        }

        try {
            transfer.saveName = saveName;
            transfer.savePath = savePath;
        } catch (...) {
            spdlog::warn(__FUNCTION__ ": could not retain save path state");
            return false;
        }

        return true;
    }

    return false;
}

void writeSaveDataPrefix(SLNet::BitStream& stream,
                         std::uint64_t saveId,
                         SaveDataOperation operation)
{
    stream.Write(static_cast<SLNet::MessageID>(ID_LOBBY_SAVE_UPLOAD));
    stream.Write(saveId);
    stream.Write(static_cast<std::uint8_t>(operation));
}

bool sendLobbyPayload(SLNet::BitStream& stream)
{
    auto service{CNetCustomService::get()};
    return service
        && service->send(stream, service->getLobbyGuid(), PacketPriority::MEDIUM_PRIORITY);
}

bool sendNativeSaveResult(std::uint64_t saveId,
                          SaveResult result,
                          const std::string& fileName = {})
{
    const bool succeeded{result == SaveResult::Success};
    if (saveId == 0 || fileName.size() > saveResultFileNameMax
        || succeeded == fileName.empty()) {
        return false;
    }

    SLNet::BitStream stream;
    stream.Write(static_cast<SLNet::MessageID>(ID_LOBBY_SAVE_NATIVE_RESULT));
    stream.Write(saveId);
    stream.Write(static_cast<std::uint8_t>(result));
    if (!fileName.empty()) {
        stream.WriteAlignedBytes(reinterpret_cast<const unsigned char*>(fileName.data()),
                                 static_cast<unsigned int>(fileName.size()));
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

bool captureSave(SaveTransferSession& transfer,
                 std::uint32_t& totalSize,
                 SaveResult& failure)
{
    const HANDLE handle{CreateFileW(transfer.savePath.c_str(), GENERIC_READ | DELETE,
                                    FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr)};
    if (handle == INVALID_HANDLE_VALUE) {
        spdlog::warn(__FUNCTION__ ": cannot open native save '{:s}', error {:d}",
                     transfer.savePath.string(), static_cast<int>(GetLastError()));
        failure = SaveResult::Failed;
        return false;
    }
    transfer.snapshotFile.reset(handle);

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(transfer.snapshotFile.get(), &fileSize) || fileSize.QuadPart <= 0) {
        failure = SaveResult::Failed;
        return false;
    }
    const auto fileSize64{static_cast<std::uint64_t>(fileSize.QuadPart)};
    if (fileSize64 > saveFileHardLimit
        || fileSize64 > std::numeric_limits<std::uint32_t>::max()) {
        failure = SaveResult::Failed;
        return false;
    }
    if (isDeadlineExpired(transfer)) {
        failure = SaveResult::TimedOut;
        return false;
    }
    if (!validateSaveSignature(transfer.snapshotFile.get())) {
        failure = SaveResult::Failed;
        return false;
    }

    totalSize = static_cast<std::uint32_t>(fileSize64);
    return true;
}

bool uploadSave(SaveTransferSession& transfer,
                std::uint32_t totalSize,
                SaveResult& failure)
{
    if (!seekToFileStart(transfer.snapshotFile.get())) {
        failure = SaveResult::Failed;
        return false;
    }

    SLNet::BitStream begin;
    writeSaveDataPrefix(begin, transfer.request.saveId, SaveDataOperation::Begin);
    begin.Write(totalSize);
    if (!sendLobbyPayload(begin)) {
        failure = SaveResult::Failed;
        return false;
    }

    std::array<unsigned char, saveChunkSizeMax> buffer{};
    for (std::uint32_t offset = 0; offset < totalSize;) {
        if (isDeadlineExpired(transfer)) {
            failure = SaveResult::TimedOut;
            return false;
        }

        const auto chunkSize{std::min(saveChunkSizeMax, totalSize - offset)};
        DWORD bytesRead{};
        if (!ReadFile(transfer.snapshotFile.get(), buffer.data(), chunkSize, &bytesRead, nullptr)
            || bytesRead != chunkSize) {
            failure = SaveResult::Failed;
            return false;
        }

        SLNet::BitStream chunk;
        writeSaveDataPrefix(chunk, transfer.request.saveId, SaveDataOperation::Chunk);
        chunk.WriteAlignedBytes(buffer.data(), chunkSize);
        if (!sendLobbyPayload(chunk)) {
            failure = SaveResult::Failed;
            return false;
        }
        offset += chunkSize;
    }

    if (isDeadlineExpired(transfer)) {
        failure = SaveResult::TimedOut;
        return false;
    }

    SLNet::BitStream commit;
    writeSaveDataPrefix(commit, transfer.request.saveId, SaveDataOperation::Commit);
    if (!sendLobbyPayload(commit)) {
        failure = SaveResult::Failed;
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

void sendLobbySaveFailure(std::uint64_t saveId, LobbyProtocol::SaveResult result)
{
    SLNet::BitStream stream;
    writeSaveDataPrefix(stream, saveId, LobbyProtocol::SaveDataOperation::Fail);
    stream.Write(static_cast<std::uint8_t>(result));
    sendLobbyPayload(stream);
}

void sendLobbySaveFailure(const LobbyProtocol::SaveRequest& request,
                          LobbyProtocol::SaveResult result)
{
    if (isLocalOnly(request)) {
        sendNativeSaveResult(request.saveId, result);
    } else {
        sendLobbySaveFailure(request.saveId, result);
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

void handleLobbySaveRequest(const LobbyProtocol::SaveRequest& request)
{
    using namespace LobbyProtocol;

    if (!isMainThread()) {
        spdlog::warn(__FUNCTION__ ": refusing capture outside the main/UI thread");
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    if (!game::CPhaseGameApi::nativeSaveSupported()) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    expireLobbySaveTransfers();
    if (request.saveId == 0 || request.saveStem.empty()) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    if (pendingStoredAck) {
        if (pendingStoredAck->saveId == request.saveId) {
            return;
        }
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    if (activeTransfer) {
        if (activeTransfer->request.saveId == request.saveId) {
            return;
        }
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }

    auto service{CNetCustomService::get()};
    auto midgard{game::CMidgardApi::get().instance()};
    auto session{service ? service->getSession() : nullptr};
    if (!service || !session || !midgard || !midgard->data || !midgard->data->multiplayerGame
        || !midgard->data->client) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    if (!clientIsNativeHost(session, midgard)) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }

    const auto sendSaveGameMsg{game::CPhaseGameApi::get().sendSaveGameMsg};
    if (!sendSaveGameMsg) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }
    const auto phaseGame{getHostPhaseGame(midgard)};
    if (!phaseGame) {
        sendLobbySaveFailure(request, SaveResult::Failed);
        return;
    }

    SaveTransferSession transfer{};
    transfer.request = request;
    transfer.deadline = std::chrono::steady_clock::now() + saveRequestTimeout;
    if (!chooseUnusedSavePath(midgard, transfer)) {
        sendLobbySaveFailure(request, SaveResult::Failed);
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
        activeTransfer.reset();
        sendLobbySaveFailure(request, SaveResult::Failed);
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
    SaveResult failure{SaveResult::Failed};
    if (!clientIsNativeHost(session, midgard)) {
        failure = SaveResult::Failed;
    } else if (isDeadlineExpired(*activeTransfer)) {
        failure = SaveResult::TimedOut;
    } else if (!success) {
        failure = SaveResult::Failed;
    } else {
        std::uint32_t totalSize{};
        if (captureSave(*activeTransfer, totalSize, failure)) {
            const auto fileName{activeTransfer->savePath.filename().string()};
            if (!sendNativeSaveResult(activeTransfer->request.saveId, SaveResult::Success,
                                      fileName)) {
                spdlog::warn(__FUNCTION__ ": failed to report native save filename {:016x}",
                             activeTransfer->request.saveId);
                failure = SaveResult::Failed;
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
        sendLobbySaveFailure(request, SaveResult::TimedOut);
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
        sendLobbySaveFailure(request, SaveResult::TimedOut);
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
