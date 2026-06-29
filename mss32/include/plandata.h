#ifndef PLANDATA_H
#define PLANDATA_H

namespace game {

/** Opaque 28-byte accumulator for visitor change records. */
struct PlanDataBuffer
{
    int data[7]{};
};

namespace PlanDataApi {

struct Api
{
    /**
     * Initialises a PlanDataBuffer.
     * sub_419A8F in Akella binary.
     * @param a2 context pointer; nullptr works for simple terrain changes.
     * @param a3 pointer to a zero-initialised int.
     * @param a4 flags; always 0 in observed call sites.
     */
    using Ctor = void*(__thiscall*)(PlanDataBuffer* thisptr, const char* a2, const int* a3, bool a4);
    Ctor ctor;

    /** Destroys a PlanDataBuffer. sub_419AEE in Akella binary. */
    using Dtor = void(__thiscall*)(PlanDataBuffer* thisptr);
    Dtor dtor;
};

Api& get();

} // namespace PlanDataApi
} // namespace game

#endif // PLANDATA_H
