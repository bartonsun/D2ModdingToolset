# Russobit native save entry point

This note records the executable evidence used by the ranked host-save path. Its support scope is
the exact binary below; identifying another executable as `GameVersion::Russobit` does not prove
that it has the same function at the same address.

## Audited executable

- File: `Discipl2.exe`
- Size: `4,187,648` bytes
- SHA-256: `1375cdef09ec470ee64fe5693fb734d7c69fb215212311d997f792b258a642eb`
- PE image base: `0x400000`

The local reference copy can be fingerprinted with:

```powershell
(Get-Item -LiteralPath .\Discipl2.exe).Length
(Get-FileHash -LiteralPath .\Discipl2.exe -Algorithm SHA256).Hash
```

The alternative Russobit/custom-icon executable of `4,214,272` bytes has not been audited for
this entry point and is outside the evidence recorded here.

The runtime ranked gate currently requires the audited `4,187,648`-byte size and the 16-byte
entry-point signature below. Computing the recorded full-file SHA-256 in-process is intentionally
deferred; it would add hashing code to a path whose immediate purpose is to reject the known
unaudited custom-icon executable. A newly encountered executable remains casual until its native
save ABI is audited and added explicitly.

## `CPhaseGame::sendSaveGameMsg`

- Virtual address: `0x40639b` (`RVA 0x639b`)
- First 16 bytes:
  `B8 BC 71 68 00 E8 2B 70 26 00 83 EC 14 56 57 8B`
- Cross-references observed at: `0x405fd6`, `0x48ffa1`, `0x4c4a69`

The disassembly reads the object from `ECX`, pushes the second stack argument followed by the
string argument, and returns with `retn 8`. This supports the declaration used by `phasegame.h`:

```cpp
void __thiscall(CPhaseGame* thisptr, const char* saveName, bool uiLockRequest);
```

This evidence establishes the address and calling convention only for the fingerprint above. It
does not establish compatible addresses for Akella, GOG, Scenario Editor, or the custom-icon
Russobit executable.

## `CCmdGameSavedMsg` view

The same binary contains the RTTI type descriptor `.?AVCCmdGameSavedMsg@@` at `0x792c10`, whose
complete-object locator is at `0x704c90` and whose vtable is at `0x6d4b44`.

Constructor `0x4786aa` establishes the fields consumed by the save callback:

- success byte at offset `+0x10`, copied from constructor argument 0;
- save-path object at offset `+0x14`, initialized/copied from argument 1;
- UI-lock byte at offset `+0x18`, copied from argument 2;
- vtable `0x6d4b44`, followed by `retn 0x0c`.

Default constructor `0x478648` also initializes the object at `+0x14` and installs the same
vtable. These observations validate the callback's local message view only for the audited SHA-256
above; the C++ layout assertions alone are not evidence for another executable.

## `CMenusReqLordMsg` serialized fields

The host handles this setup request through local loopback, so the custom lobby forwards the lord
selection separately. The forwarded value is taken from the second 32-bit field after
`NetMessageHeader`, based on these observations in the same audited executable:

- RTTI type descriptor `.?AVCMenusReqLordMsg@@` is at `0x7972f0`, with vtable `0x6d4ffc`;
- constructor `0x47d0c2` stores the category id at object offset `+0x4` and the lord index at
  `+0x8`;
- serializer `0x47d154` first calls the base serializer, then writes four bytes from `+0x4` and
  four bytes from `+0x8`, in that order;
- server handler `0x42b977` resolves the category from `+0x4` and passes the value returned by
  accessor `0x5ffc5f` (`this + 0x8`) to `0x42b428`; that function stores the value at player-state
  offset `+0xc` while copying the category separately.

Consequently, reading the first 32-bit field after the 44-byte header would forward the category,
not the lord. As with the save ABI above, these addresses document only the audited executable.
