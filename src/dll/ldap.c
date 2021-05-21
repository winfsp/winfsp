/**
 * @file dll/ldap.c
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

#include <dll/library.h>
#include <winldap.h>

ULONG FspLdapConnect(PWSTR HostName, PVOID *PLdap)
{
    LDAP *Ldap = 0;
    ULONG LdapResult;

    *PLdap = 0;

    Ldap = ldap_initW(HostName, LDAP_PORT);
    if (0 == Ldap)
    {
        LdapResult = LdapGetLastError();
        goto exit;
    }

    /* enable signing and encryption */
    ldap_set_optionW(Ldap, LDAP_OPT_SIGN, LDAP_OPT_ON);
    ldap_set_optionW(Ldap, LDAP_OPT_ENCRYPT, LDAP_OPT_ON);

    LdapResult = ldap_bind_sW(Ldap, 0, 0, LDAP_AUTH_NEGOTIATE);
    if (LDAP_SUCCESS != LdapResult)
        goto exit;

    *PLdap = Ldap;
    LdapResult = LDAP_SUCCESS;

exit:
    if (LDAP_SUCCESS != LdapResult)
    {
        if (0 != Ldap)
            ldap_unbind(Ldap);
    }

    return LdapResult;
}

VOID FspLdapClose(PVOID Ldap0)
{
    LDAP *Ldap = Ldap0;

    ldap_unbind(Ldap);
}

ULONG FspLdapGetValue(PVOID Ldap0, PWSTR Base, ULONG Scope, PWSTR Filter, PWSTR Attribute,
    PWSTR *PValue)
{
    LDAP *Ldap = Ldap0;
    PWSTR Attributes[2];
    LDAPMessage *Message = 0, *Entry;
    PWSTR *Values = 0;
    int Size;
    PWSTR Value;
    ULONG LdapResult;

    *PValue = 0;

    Attributes[0] = Attribute;
    Attributes[1] = 0;
    LdapResult = ldap_search_sW(Ldap, Base, Scope, Filter, Attributes, 0, &Message);
    if (LDAP_SUCCESS != LdapResult)
        goto exit;

    Entry = ldap_first_entry(Ldap, Message);
    if (0 == Entry)
    {
        LdapResult = LDAP_OTHER;
        goto exit;
    }

    Values = ldap_get_valuesW(Ldap, Entry, Attributes[0]);
    if (0 == Values || 0 == ldap_count_valuesW(Values))
    {
        LdapResult = LDAP_OTHER;
        goto exit;
    }

    Size = (lstrlenW(Values[0]) + 1) * sizeof(WCHAR);
    Value = MemAlloc(Size);
    if (0 == Value)
    {
        LdapResult = LDAP_NO_MEMORY;
        goto exit;
    }
    memcpy(Value, Values[0], Size);

    *PValue = Value;
    LdapResult = LDAP_SUCCESS;

exit:
    if (0 != Values)
        ldap_value_freeW(Values);
    if (0 != Message)
        ldap_msgfree(Message);

    return LdapResult;
}

ULONG FspLdapGetDefaultNamingContext(PVOID Ldap, PWSTR *PValue)
{
    return FspLdapGetValue(Ldap, 0, LDAP_SCOPE_BASE, L"(objectClass=*)", L"defaultNamingContext",
        PValue);
}

ULONG FspLdapGetTrustPosixOffset(PVOID Ldap, PWSTR Context, PWSTR Domain, PWSTR *PValue)
{
    WCHAR Base[1024];
    WCHAR Filter[512];
    BOOLEAN IsFlatName;

    *PValue = 0;

    if (sizeof Base / sizeof Base[0] - 64 < lstrlenW(Context) ||
        sizeof Filter / sizeof Filter[0] - 64 < lstrlenW(Domain))
        return LDAP_OTHER;

    IsFlatName = TRUE;
    for (PWSTR P = Domain; *P; P++)
        if (L'.' == *P)
        {
            IsFlatName = FALSE;
            break;
        }

    wsprintfW(Base,
        L"CN=System,%s",
        Context);
    wsprintfW(Filter,
        IsFlatName ?
            L"(&(objectClass=trustedDomain)(flatName=%s))" :
            L"(&(objectClass=trustedDomain)(name=%s))",
        Domain);

    return FspLdapGetValue(Ldap, Base, LDAP_SCOPE_ONELEVEL, Filter, L"trustPosixOffset", PValue);
}
