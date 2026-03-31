#include "global.h"
#include "game/globals.h"
#include "game/sa2/input_buffer.h"
#include "game/sa1_sa2_shared/player.h"

#include "data/input_combos.h"

UNUSED u32 unused_030055C0[4] = {};

u8 gFrameInputsBuf[4] = {};
u8 gNewInputCountersIndex = 0;
u8 ALIGNED(4) gFrameInputsBufIndex = 0;

// Fills available space, but size not yet confirmed
struct InputCounters ALIGNED(8) gNewInputCounters[32] = {};

// Trick input patterns
const u8 gUnknown_080D5254[] = { 0x04, 0x01, 0x00, 0xF0, 0x08, 0x10, 0xF0, 0x08, 0x00, 0xF0, 0x08, 0x10, 0xF0, 0x01 };
const u8 gUnknown_080D5262[] = {
    0x04, // r6
    0x01, // r8

    // 0, 1, 2
    0x00, 0xF0, 0x08, //
    0x20, 0xF0, 0x08, //
    0x00, 0xF0, 0x08, //
    0x20, 0xF0, 0x01, //
};
const u8 gUnknown_080D5270[] = { 0x04, 0x01, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D527E[] = { 0x04, 0x02, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D528C[] = { 0x04, 0x01, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D529A[] = { 0x04, 0x02, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D52A8[] = { 0x04, 0x01, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D52B6[] = { 0x04, 0x02, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D52C4[] = { 0x04, 0x01, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };
const u8 gUnknown_080D52D2[] = { 0x04, 0x02, 0x10, 0xF0, 0x0F, 0x00, 0xF0, 0x0F, 0x10, 0xF0, 0x0F, 0x03, 0x07, 0x0F };

// The current value in gNewInputCounters[gNewInputCountersIndex]
// gets increased until either it reaches 0xFF or a new button was pressed.
// Letting go of a button does not trigger the index increase.
//
// This is likely used for trick timings
void InputBuffer_HandleFrameInput(Player *p)
{
    const u8 **unk0;
    const u8 *data;

    if (p->moveState & (MOVESTATE_IGNORE_INPUT | MOVESTATE_DEAD)) {
        InputBuffer_NewFrameInput(0, 0);
        return;
    }
    // _0800DFB4

    InputBuffer_NewFrameInput(p->frameInput, p->frameInput);

    if (p->unk70 != 0) {
        u16 unk4 = 0;
        unk0 = gUnknown_08C871D4[p->character];
        data = *unk0;
        if (data != INPUTBUF_NULL_PTR) {
            // __0800DFEC
            u32 r6 = *data++;
            u32 r8 = *data++;

            // _0800E002
            while (p->unk71 != r8) {
                u8 cid = gNewInputCountersIndex;

                // _0800E012
                while (r6 != 0) {
                    u32 maskFF = 0xFF, mask1F = 0x1F;
                    bool32 shouldContinue;

                    s16 r1;
                    u32 r7, ip;
                    r6 = (u16)(r6 - 1);

                    ip = data[r6 * 3 + 0];
                    r7 = data[r6 * 3 + 1];
                    r1 = data[r6 * 3 + 2] & maskFF;

                    // _0800E02A
                    shouldContinue = FALSE;
                    while (1) {
                        s32 r0;

                        // TODO: use the permuter to find the real match for this
                        // https://decomp.me/scratch/eLocX
#ifndef NON_MATCHING
                        register s32 r3 asm("r3");
                        register s32 r2 asm("r2");
#else
                        s32 r2, r3;
#endif
                        r3 = gNewInputCounters[cid].unk0 & r7;
                        r2 = gNewInputCounters[cid].unk1;

                        r0 = r1;
                        r2 &= maskFF;
                        if (r0 >= r2) {
                            if (ip == r3) {
                                cid = (cid - 1) & mask1F;
                                shouldContinue = TRUE;
                                break;
                            }
                            cid = (cid - 1) & mask1F;
                            r0 -= 1;
                            r0 -= r2;
                            r1 = r0;
                        } else {
                            break;
                        }
                    }

                    if (shouldContinue == FALSE) {
                        break;
                    }
                }

                if (r6 == 0) {
                    p->unk71 = r8;
                    break;
                }

                unk4++;
                data = unk0[unk4];
                if (data == INPUTBUF_NULL_PTR) {
                    break;
                }
                r6 = *data++;
                r8 = *data++;
            }
        }
    }
}

// (100.00%) https://decomp.me/scratch/WjLUa
void InputBuffer_NewFrameInput(u16 frameInputs1, u16 frameInputs2)
{
    u32 r3 = frameInputs2 % 4u;
    u16 r2 = ((frameInputs2 & R_BUTTON) >> 6);
    u32 r5;

    r3 |= r2;
    frameInputs2 = 0xF0;
#ifndef NON_MATCHING
    asm("" ::"r"(frameInputs2));
#endif
    r5 = frameInputs1;
    r5 &= frameInputs2;
    r5 = (r5) | r3;
    gFrameInputsBufIndex = (gFrameInputsBufIndex + 1) % ARRAY_COUNT(gFrameInputsBuf);
    gFrameInputsBuf[gFrameInputsBufIndex] = r5;
    r5 |= gFrameInputsBuf[(gFrameInputsBufIndex - 1) % ARRAY_COUNT(gFrameInputsBuf)];
    r5 |= gFrameInputsBuf[(gFrameInputsBufIndex - 2) % ARRAY_COUNT(gFrameInputsBuf)];
    r5 &= frameInputs1;

#ifndef NON_MATCHING
    asm("lsl r0, %0, #24\n"
        "lsr %0, r0, #24\n"
        : "=r"(r5)
        : "r"(r5));
#endif

    if ((gNewInputCounters[gNewInputCountersIndex].unk0 == r5) && (gNewInputCounters[gNewInputCountersIndex].unk1 != 0xFF)) {
        gNewInputCounters[gNewInputCountersIndex].unk1++;
    } else {
        gNewInputCountersIndex = (gNewInputCountersIndex + 1) % ARRAY_COUNT(gNewInputCounters);
        gNewInputCounters[gNewInputCountersIndex].unk0 = r5;
        gNewInputCounters[gNewInputCountersIndex].unk1 = 0;
    }
}

void InitNewInputCounters(void)
{
    gNewInputCountersIndex = 0;
    DmaFill32(3, 0, gNewInputCounters, sizeof(gNewInputCounters) - 4);
}
