/**
 * @file shared/ku/uuid5.c
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

#include <shared/ku/library.h>
#include <bcrypt.h>

/*
 * This module is used to create UUID v5 identifiers. UUID v5 identifiers
 * are effectively SHA1 hashes that are modified to fit within the UUID
 * format. The resulting identifiers use version 5 and variant 2. The hash
 * is taken over the concatenation of a namespace ID and a name; the namespace
 * ID is another UUID and the name can be any string of bytes ("octets").
 *
 * For details see RFC 4122: https://tools.ietf.org/html/rfc4122
 */

NTSTATUS FspUuid5Make(const UUID *Namespace, const VOID *Buffer, ULONG Size, UUID *Uuid)
{
    BCRYPT_ALG_HANDLE ProvHandle = 0;
    BCRYPT_HASH_HANDLE HashHandle = 0;
    UINT8 Temp[20];
    NTSTATUS Result;

    /*
     * Windows UUID's are encoded in little-endian format. RFC 4122 specifies that for
     * UUID v5 computation, UUID's must be converted to/from big-endian.
     *
     * Note that Windows is always little-endian:
     * https://community.osr.com/discussion/comment/146810/#Comment_146810
     */

    /* copy Namespace to local buffer in network byte order (big-endian) */
    Temp[ 0] = ((PUINT8)Namespace)[ 3];
    Temp[ 1] = ((PUINT8)Namespace)[ 2];
    Temp[ 2] = ((PUINT8)Namespace)[ 1];
    Temp[ 3] = ((PUINT8)Namespace)[ 0];
    Temp[ 4] = ((PUINT8)Namespace)[ 5];
    Temp[ 5] = ((PUINT8)Namespace)[ 4];
    Temp[ 6] = ((PUINT8)Namespace)[ 7];
    Temp[ 7] = ((PUINT8)Namespace)[ 6];
    Temp[ 8] = ((PUINT8)Namespace)[ 8];
    Temp[ 9] = ((PUINT8)Namespace)[ 9];
    Temp[10] = ((PUINT8)Namespace)[10];
    Temp[11] = ((PUINT8)Namespace)[11];
    Temp[12] = ((PUINT8)Namespace)[12];
    Temp[13] = ((PUINT8)Namespace)[13];
    Temp[14] = ((PUINT8)Namespace)[14];
    Temp[15] = ((PUINT8)Namespace)[15];

    /*
     * Unfortunately we cannot reuse the hashing object, because BCRYPT_HASH_REUSABLE_FLAG
     * is available in Windows 8 and later. (WinFsp currently supports Windows 7 or later).
     */

    Result = BCryptOpenAlgorithmProvider(&ProvHandle, BCRYPT_SHA1_ALGORITHM, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = BCryptCreateHash(ProvHandle, &HashHandle, 0, 0, 0, 0, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = BCryptHashData(HashHandle, (PVOID)Temp, 16, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = BCryptHashData(HashHandle, (PVOID)Buffer, Size, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    Result = BCryptFinishHash(HashHandle, Temp, 20, 0);
    if (!NT_SUCCESS(Result))
        goto exit;

    /* copy local buffer to Uuid in host byte order (little-endian) */
    ((PUINT8)Uuid)[ 0] = Temp[ 3];
    ((PUINT8)Uuid)[ 1] = Temp[ 2];
    ((PUINT8)Uuid)[ 2] = Temp[ 1];
    ((PUINT8)Uuid)[ 3] = Temp[ 0];
    ((PUINT8)Uuid)[ 4] = Temp[ 5];
    ((PUINT8)Uuid)[ 5] = Temp[ 4];
    ((PUINT8)Uuid)[ 6] = Temp[ 7];
    ((PUINT8)Uuid)[ 7] = Temp[ 6];
    ((PUINT8)Uuid)[ 8] = Temp[ 8];
    ((PUINT8)Uuid)[ 9] = Temp[ 9];
    ((PUINT8)Uuid)[10] = Temp[10];
    ((PUINT8)Uuid)[11] = Temp[11];
    ((PUINT8)Uuid)[12] = Temp[12];
    ((PUINT8)Uuid)[13] = Temp[13];
    ((PUINT8)Uuid)[14] = Temp[14];
    ((PUINT8)Uuid)[15] = Temp[15];

    /* [RFC 4122 Section 4.3]
     * Set the four most significant bits (bits 12 through 15) of the
     * time_hi_and_version field to the appropriate 4-bit version number
     * from Section 4.1.3.
     */
    Uuid->Data3 = (5 << 12) | (Uuid->Data3 & 0x0fff);

    /* [RFC 4122 Section 4.3]
     * Set the two most significant bits (bits 6 and 7) of the
     * clock_seq_hi_and_reserved to zero and one, respectively.
     */
    Uuid->Data4[0] = (2 << 6) | (Uuid->Data4[0] & 0x3f);

    Result = STATUS_SUCCESS;

exit:
    if (0 != HashHandle)
        BCryptDestroyHash(HashHandle);

    if (0 != ProvHandle)
        BCryptCloseAlgorithmProvider(ProvHandle, 0);

    return Result;
}
