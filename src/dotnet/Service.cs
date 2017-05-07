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
        public Service(String ServiceName)
        {
            Api.FspServiceCreate(ServiceName, _OnStart, _OnStop, null, out _ServicePtr);
            if (IntPtr.Zero != _ServicePtr)
                Api.SetUserContext(_ServicePtr, this);
        }
        ~Service()
        {
            if (IntPtr.Zero != _ServicePtr)
            {
                Api.DisposeUserContext(_ServicePtr);
                Api.FspServiceDelete(_ServicePtr);
            }
        }

        /* control */
        public int Run()
        {
            if (IntPtr.Zero == _ServicePtr)
            {
                const Int32 STATUS_INSUFFICIENT_RESOURCES = unchecked((Int32)0xc000009a);
                Log(EVENTLOG_ERROR_TYPE,
                    String.Format("The service {0} cannot be created (Status={1:X}).",
                    GetType().FullName, STATUS_INSUFFICIENT_RESOURCES));
                return (int)Api.FspWin32FromNtStatus(STATUS_INSUFFICIENT_RESOURCES);
            }
            Api.FspServiceAllowConsoleMode(_ServicePtr);
            Int32 Result = Api.FspServiceLoop(_ServicePtr);
            int ExitCode = (int)Api.FspServiceGetExitCode(_ServicePtr);
            if (0 > Result)
            {
                Log(EVENTLOG_ERROR_TYPE,
                    String.Format("The service {0} has failed to run (Status={1:X}).",
                    GetType().FullName, Result));
                return (int)Api.FspWin32FromNtStatus(Result);
            }
            return ExitCode;
        }
        public void Stop()
        {
            if (IntPtr.Zero == _ServicePtr)
                throw new InvalidOperationException();
            Api.FspServiceStop(_ServicePtr);
        }
        public void RequestTime(UInt32 Time)
        {
            if (IntPtr.Zero == _ServicePtr)
                throw new InvalidOperationException();
            Api.FspServiceRequestTime(_ServicePtr, Time);
        }
        public int ExitCode
        {
            get
            {
                if (IntPtr.Zero == _ServicePtr)
                    throw new InvalidOperationException();
                return (int)Api.FspServiceGetExitCode(_ServicePtr);
            }
            set
            {
                if (IntPtr.Zero == _ServicePtr)
                    throw new InvalidOperationException();
                Api.FspServiceSetExitCode(_ServicePtr, (UInt32)value);
            }
        }
        public IntPtr ServiceHandle
        {
            get { return _ServicePtr; }
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

        private static Api.Proto.ServiceStart _OnStart = OnStart;
        private static Api.Proto.ServiceStop _OnStop = OnStop;
        private IntPtr _ServicePtr;
    }

}
