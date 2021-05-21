/**
 * @file shared/ku/config.h
 *
 * Shared kernel/user configuration. This file is to be included by the
 * FSD and DLL components ONLY!
 *
 * @copyright 2015-2021 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#ifndef WINFSP_SHARED_KU_CONFIG_H_INCLUDED
#define WINFSP_SHARED_KU_CONFIG_H_INCLUDED

/*
 * Define the FSP_CFG_REJECT_EARLY_IRP macro to support the RejectIrpPriorToTransact0 flag.
 */
#define FSP_CFG_REJECT_EARLY_IRP

#endif
