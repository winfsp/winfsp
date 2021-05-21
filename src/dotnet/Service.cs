/*
 * dotnet/Service.cs
 *
 * Copyright 2015-2021 Bill Zissimopoulos
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

using System;

using Fsp.Interop;

namespace Fsp
{

    /// <summary>
    /// Provides the base class for a process that can be run as a service,
    /// command line application or under the control of the WinFsp launcher.
    /// </summary>
    public class Service
    {
        /* const */
        public const UInt32 EVENTLOG_ERROR_TYPE = 0x0001;
        public const UInt32 EVENTLOG_WARNING_TYPE = 0x0002;
        public const UInt32 EVENTLOG_INFORMATION_TYPE = 0x0004;

        /* ctor/dtor */
        /// <summary>
        /// Creates an instance of the Service class.
        /// </summary>
        /// <param name="ServiceName">The name of the service.</param>
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
        /// <summary>
        /// Runs a service.
        /// </summary>
        /// <returns>Service process exit code.</returns>
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
        /// <summary>
        /// Stops a running service.
        /// </summary>
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
        /// <summary>
        /// Gets or sets the service process exit code.
        /// </summary>
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
        /// <summary>
        /// Provides a means to customize the returned status code when an exception happens.
        /// </summary>
        /// <param name="ex"></param>
        /// <returns>STATUS_SUCCESS or error code.</returns>
        protected virtual Int32 ExceptionHandler(Exception ex)
        {
            return unchecked((Int32)0xE0434f4D)/*STATUS_CLR_EXCEPTION*/;
        }
        /// <summary>
        /// Occurs when the service starts.
        /// </summary>
        /// <param name="Args">Command line arguments passed to the service.</param>
        protected virtual void OnStart(String[] Args)
        {
        }
        /// <summary>
        /// Occurs when the service stops.
        /// </summary>
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
