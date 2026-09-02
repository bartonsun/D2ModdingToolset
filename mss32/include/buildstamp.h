/*
 * This file is part of the modding toolset for Disciples 2.
 *
 * A single greppable identifier for the binary a player is running.
 *
 * The install line writes the game version enum, which is the same in every
 * build we ever shipped, so two logs from two different dlls of ours read
 * identically. Measured 2026-09-01: a client reported the shipped fix as not
 * working, and his log could not tell us whether he had replaced the file.
 * The stamp is bumped by hand whenever a build leaves for a client.
 */

#ifndef BUILDSTAMP_H
#define BUILDSTAMP_H

namespace hooks {

inline constexpr char buildStamp[] = "t55-20260902";

} // namespace hooks

#endif // BUILDSTAMP_H
