/*
 * This file is part of the modding toolset for Disciples 2.
 * (https://github.com/VladimirMakeev/D2ModdingToolset)
 * Copyright (C) 2021 Stanislav Egorov.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef UIMANAGER_H
#define UIMANAGER_H

#include "d2assert.h"
#include "smartptr.h"
#include <cstdint>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace game {

struct CMqPoint;
struct CMqUIControllerSimple;
struct CMqUIKernelSimple;
struct UiEvent;

struct CUIManagerData
{
    CMqUIControllerSimple* uiController;
    CMqUIKernelSimple* uiKernel;
};

assert_size(CUIManagerData, 8);

struct CUIManager
{
    const void* vftable;
    CUIManagerData* data;
};

assert_size(CUIManager, 8);

using UIManagerPtr = SmartPtr<CUIManager>;

namespace CUIManagerApi {

struct Api
{
    using Get = UIManagerPtr*(__stdcall*)(UIManagerPtr* managerPtr);
    Get get;

    template <typename Callback>
    using CreateEventFunctor = SmartPointer*(__stdcall*)(SmartPointer* functor,
                                                         int zero,
                                                         void* userData,
                                                         Callback* callback);

    struct TimerEventCallback
    {
        using Callback = void(__thiscall*)(void* thisptr);
        Callback callback;
        int unknown;
    };

    /**
     * Creates functor to be used in timer event.
     * All functor creating functions here are reused since they implement the same logic.
     * The only difference is the template arguments that were used in the original game code.
     */
    using CreateTimerEventFunctor = CreateEventFunctor<TimerEventCallback>;
    CreateTimerEventFunctor createTimerEventFunctor;

    /**
     * Creates event with user specified callback to be called periodically.
     * Creates uiEvent with type UiEventType::Timer.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event. Actual type SmartPtr<CBFunctorDispatch0>.
     * @param timeoutMs event triggering rate, in milliseconds.
     * @returns pointer to uiEvent.
     */
    using CreateTimerEvent = UiEvent*(__thiscall*)(CUIManager* thisptr,
                                                   UiEvent* uiEvent,
                                                   SmartPointer* functor,
                                                   std::uint32_t timeoutMs);
    CreateTimerEvent createTimerEvent;

    struct UpdateEventCallback
    {
        using Callback = bool(__thiscall*)(void* thisptr);
        Callback callback;
    };

    /** Creates functor to be used in update event. */
    using CreateUpdateEventFunctor = CreateEventFunctor<UpdateEventCallback>;
    CreateUpdateEventFunctor createUpdateEventFunctor;

    using CreateUiEvent = UiEvent*(__thiscall*)(CUIManager* thisptr,
                                                UiEvent* uiEvent,
                                                SmartPointer* functor);

    /**
     * Creates event with constant updates, callback is called from game message processing loop.
     * Creates uiEvent with type UiEventType::Update.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch0wRet<bool>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createUpdateEvent;

    /**
     * Creates event with user callback that is called when window loses or receives focus.
     * Creates uiEvent with type UiEventType::VisibilityChange.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch1<bool>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createVisibilityChangeEvent;

    /**
     * Creates event with user callback that is called when keyboard key is pressed.
     * Creates uiEvent with type UiEventType::KeyPress.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch3<unsigned int, unsigned int, unsigned int>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createKeypressEvent;

    /**
     * Creates event with user callback that is called when mouse key is pressed.
     * Creates uiEvent with type UiEventType::MousePress.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch3<unsigned short, unsigned short, const tagPOINT &>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createMousePressEvent;

    /**
     * Creates event with user callback that is called when mouse changes position.
     * Creates uiEvent with type UiEventType::MouseButtonPress.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch2<unsigned short, const tagPOINT &>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createMouseMoveEvent;

    /**
     * Creates event with user callback that is called when player attempts to close game window.
     * Creates uiEvent with type UiEventType::Close.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch0wRet<bool>>.
     * @returns pointer to uiEvent.
     */
    CreateUiEvent createCloseEvent;

    using MessageEventCallback = void(__thiscall*)(void* thisptr, unsigned int, long);

    /** Creates functor to be used in message event. */
    using CreateMessageEventFunctor = CreateEventFunctor<MessageEventCallback>;
    CreateMessageEventFunctor createMessageEventFunctor;

    /**
     * Creates event with user callback that is called when message previously created by
     * RegisterMessage is received.
     * Creates uiEvent with type UiEventType::Message.
     * @param[inout] uiEvent event structure to store result. Caller is responsible to call
     * destructor.
     * @param[in] functor callback associated with event.
     * Actual type SmartPtr<CBFunctorDispatch2<unsigned int, long>>.
     * @param messageId message identifier from RegisterWindowMessage.
     * @returns pointer to uiEvent.
     */
    using CreateMessageEvent = UiEvent*(__thiscall*)(CUIManager* thisptr,
                                                     UiEvent* uiEvent,
                                                     SmartPointer* functor,
                                                     std::uint32_t messageId);
    CreateMessageEvent createMessageEvent;

    using GetMousePosition = CMqPoint*(__thiscall*)(const CUIManager* thisptr, CMqPoint* value);
    GetMousePosition getMousePosition;

    /**
     * Registers message with specified name for use in message events.
     * @param[in] messageName name of message to register.
     * @returns message identifier.
     */
    using RegisterMessage = std::uint32_t(__thiscall*)(CUIManager* thisptr,
                                                       const char* messageName);
    RegisterMessage registerMessage;

    using PostWndMessage = bool(__thiscall*)(const CUIManager* thisptr,
                                             UINT messageId,
                                             WPARAM wParam,
                                             LPARAM lParam);
    PostWndMessage postMessage;
};

Api& get();

} // namespace CUIManagerApi

} // namespace game

#endif // UIMANAGER_H
