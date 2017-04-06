/**
 * @file dotnet/Service.cs
 *
 * @copyright 2015-2017 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

using System;
using System.Runtime.InteropServices;

using Fsp.Interop;

namespace Fsp
{

    public class Service
    {
        /* const */
        public const UInt32 EVENTLOG_ERROR_TYPE = 0x0001;
        public const UInt32 EVENTLOG_WARNING_TYPE = 0x0002;
        public const UInt32 EVENTLOG_INFORMATION_TYPE = 0x0004;

        /* ctor/dtor */
        Service(String ServiceName)
        {
            _CreateResult = Api.FspServiceCreate(ServiceName, OnStart, OnStop, null, out _Service);
            Api.SetUserContext(_Service, this);
        }
        ~Service()
        {
            if (IntPtr.Zero != _Service)
            {
                Api.SetUserContext(_Service, null);
                Api.FspServiceDelete(_Service);
            }
        }

        /* control */
        public int Run()
        {
            if (0 > _CreateResult)
            {
                Log(EVENTLOG_ERROR_TYPE,
                    String.Format("The service cannot be created (Status={0:X}).", _CreateResult));
                return (int)Api.FspWin32FromNtStatus(_CreateResult);
            }
            Api.FspServiceAllowConsoleMode(_Service);
            Int32 Result = Api.FspServiceLoop(_Service);
            UInt32 ExitCode = Api.FspServiceGetExitCode(_Service);
            if (0 > _CreateResult)
            {
                Log(EVENTLOG_ERROR_TYPE,
                    String.Format("The service has failed to run (Status={0:X}).", Result));
                return (int)Api.FspWin32FromNtStatus(Result);
            }
            return (int)ExitCode;
        }
        public void Stop()
        {
            if (0 > _CreateResult)
                return;
            Api.FspServiceStop(_Service);
        }
        public void RequestTime(UInt32 Time)
        {
            Api.FspServiceRequestTime(_Service, Time);
        }
        public int ExitCode
        {
            get { return (int)Api.FspServiceGetExitCode(_Service); }
            set { Api.FspServiceSetExitCode(_Service, (UInt32)value); }
        }
        public static void Log(UInt32 Type, String Message)
        {
            Api.FspServiceLog(Type, "%s", Message);
        }

        /* start/stop */
        protected virtual Int32 ExceptionHandler(Exception ex)
        {
            return unchecked((Int32)0xE0434f4D)/*STATUS_CLR_EXCEPTION*/;
        }
        protected virtual void OnStart(String[] Args)
        {
        }
        protected virtual void OnStop()
        {
        }

        /* callbacks */
        private static Int32 OnStart(
            IntPtr Service,
            UInt32 Argc,
            String[] Argv)
        {
            Service self = (Service)Api.GetUserContext(Service);
            try
            {
                self.OnStart(
                    Argv);
                return 0/*STATUS_SUCCESS*/;
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }
        private static Int32 OnStop(
            IntPtr Service)
        {
            Service self = (Service)Api.GetUserContext(Service);
            try
            {
                self.OnStop();
                return 0/*STATUS_SUCCESS*/;
            }
            catch (Exception ex)
            {
                return self.ExceptionHandler(ex);
            }
        }

        private IntPtr _Service;
        private Int32 _CreateResult;
    }

}
