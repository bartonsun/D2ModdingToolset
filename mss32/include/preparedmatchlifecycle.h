#ifndef PREPAREDMATCHLIFECYCLE_H
#define PREPAREDMATCHLIFECYCLE_H
#include "preparedmatchprotocol.h"
#include <algorithm>
#include <deque>
#include <optional>

namespace hooks::prepared {
enum class Stage { Waiting, Confirming, Accepted, Generating, Creating, Setup, Terminal, Returning };
enum class CancelAction { Acknowledge, AwaitSafePoint, PreserveRoom };
enum class JoinStage { Waiting, CheckingRoom, Prompt, Accepted, CheckingJoinRoom, Terminal };
enum class JoinAction { Wait, RefreshRooms, ShowPrompt, Join, Unavailable };

inline JoinStage joinStageAfterBusy(JoinStage stage)
{
    if (stage == JoinStage::CheckingRoom) return JoinStage::Waiting;
    if (stage == JoinStage::CheckingJoinRoom) return JoinStage::Accepted;
    return stage;
}

class JoinReceipts {
    struct Receipt { JoinIdentity target; std::string recipient; JoinState state; };
    std::deque<Receipt> values;
public:
    std::optional<JoinState> find(const JoinIdentity& target, const std::string& recipient) const {
        const auto found = std::find_if(values.begin(), values.end(), [&](const Receipt& v) {
            return v.target == target && v.recipient == recipient;
        });
        return found == values.end() ? std::nullopt : std::optional<JoinState>(found->state);
    }
    void remember(const JoinIdentity& target, const std::string& recipient, JoinState state) {
        if (state != JoinState::Accepted && state != JoinState::Declined && state != JoinState::Unavailable) return;
        if (find(target, recipient)) return;
        values.push_back({target, recipient, state});
        if (values.size() > 512) values.pop_front();
    }
};

// Called only after the native idle-lobby check.
inline JoinAction joinAction(JoinStage stage, bool freshRooms, bool roomAvailable)
{
    if (stage == JoinStage::Terminal) return JoinAction::Wait;
    if (stage == JoinStage::Waiting || stage == JoinStage::Accepted) return JoinAction::RefreshRooms;
    if (stage == JoinStage::Prompt) return JoinAction::ShowPrompt;
    if (!freshRooms) return JoinAction::Wait;
    if (!roomAvailable) return JoinAction::Unavailable;
    return stage == JoinStage::CheckingJoinRoom ? JoinAction::Join : JoinAction::ShowPrompt;
}

inline CancelAction cancelAction(Stage stage, bool roomCreated)
{
    if (stage == Stage::Generating || stage == Stage::Creating)
        return CancelAction::AwaitSafePoint;
    if (stage == Stage::Setup || roomCreated) return CancelAction::PreserveRoom;
    return CancelAction::Acknowledge;
}

inline CancelAction requestCancellation(Stage stage, bool roomCreated, bool& canceled)
{
    const auto action = cancelAction(stage, roomCreated);
    // A late Cancel must not suppress the created room's native setup.
    if (action != CancelAction::PreserveRoom) canceled = true;
    return action;
}

inline Stage stageAfterRoomCreationResult(bool success, bool& canceled)
{
    if (!success) return Stage::Returning;
    // CreateRoom won the race with Cancel.
    canceled = false;
    return Stage::Setup;
}

inline Stage stageAfterCanceledAck(Stage stage)
{
    // A late/duplicate cancel must not discard the pending native menu cleanup.
    return stage == Stage::Returning ? Stage::Returning : Stage::Terminal;
}
} // namespace hooks::prepared
#endif
