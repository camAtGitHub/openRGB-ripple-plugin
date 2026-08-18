/*---------------------------------------------------------*\
| DetectionLifecycle.h                                      |
|   Host-less resume rules for OpenRGB detection callbacks. |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <cstdint>

/* DetectionStart sets paused and bumps suspend_epoch. DetectionEnd
   captures that epoch, then the GUI slot must resume.

   1.0.4 treated paused==true as "a newer DetectionStart raced us".
   paused is set true at the start of THIS cycle, so that check made
   resume unreachable and left the effect dead after every Rescan.

   A stale DetectionEnd is only one whose captured epoch no longer
   matches suspend_epoch (a newer Start happened in between). */
inline bool DetectionEndIsCurrent(uint64_t queued_end_epoch,
                                  uint64_t suspend_epoch)
{
    return queued_end_epoch == suspend_epoch;
}
