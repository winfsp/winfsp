using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace MemFsSharp
{
    class NtStatus 
    {
        //NT STATUS
        public const uint STATUS_SUCCESS = 0x00000000;

        public const uint STATUS_MORE_ENTRIES = 0x00000105;
        public const uint STATUS_NO_SUCH_DEVICE = 0xC000000E;
        public const uint STATUS_NO_SUCH_FILE = 0xC000000F;
        public const uint STATUS_NO_MORE_FILES = 0x80000006;
        public const uint STATUS_OBJECT_NAME_INVALID = 0xC0000033;
        public const uint STATUS_OBJECT_NAME_NOT_FOUND = 0xC0000034;
        public const uint STATUS_OBJECT_NAME_COLLISION = 0xC0000035;
        public const uint STATUS_OBJECT_PATH_NOT_FOUND = 0xC000003A;
        public const uint STATUS_FILE_IS_A_DIRECTORY = 0xC00000BA;
        public const uint STATUS_NOT_A_DIRECTORY = 0xC0000103;
        public const uint STATUS_NOT_SUPPORTED = 0xC00000BB;
        public const uint STATUS_NONCONTINUABLE_EXCEPTION = 0xC0000025;
        public const uint STATUS_BUFFER_OVERFLOW = 0x80000005;
        public const uint STATUS_DIRECTORY_NOT_EMPTY = 0xC0000101;
        public const uint STATUS_END_OF_FILE = 0xC0000011;
        public const uint STATUS_NOT_IMPLEMENTED = 0xC0000002;
        public const uint STATUS_REQUEST_NOT_ACCEPTED = 0xC00000D0;
        public const uint STATUS_INVALID_PARAMETER = 0xC000000D;
        public const uint STATUS_ACCESS_DENIED = 0xC0000022;
    }
    class FileAttirutes {

        public const uint FILE_ATTRIBUTE_READONLY = 0x00000001;
        public const uint FILE_ATTRIBUTE_HIDDEN = 0x00000002;
        public const uint FILE_ATTRIBUTE_SYSTEM = 0x00000004;
        public const uint FILE_ATTRIBUTE_DIRECTORY = 0x00000010;
        public const uint FILE_ATTRIBUTE_ARCHIVE = 0x00000020;
        public const uint FILE_ATTRIBUTE_DEVICE = 0x00000040;
        public const uint FILE_ATTRIBUTE_NORMAL = 0x00000080;
        public const uint FILE_ATTRIBUTE_TEMPORARY = 0x00000100;
        public const uint FILE_ATTRIBUTE_SPARSE_FILE = 0x00000200;
        public const uint FILE_ATTRIBUTE_REPARSE_POINT = 0x00000400;
        public const uint FILE_ATTRIBUTE_COMPRESSED = 0x00000800;
        public const uint FILE_ATTRIBUTE_OFFLINE = 0x00001000;
        public const uint FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x00002000;
        public const uint FILE_ATTRIBUTE_ENCRYPTED = 0x00004000;
        public const uint FILE_ATTRIBUTE_INTEGRITY_STREAM = 0x00008000;
        public const uint FILE_ATTRIBUTE_VIRTUAL = 0x00010000;
        public const uint FILE_ATTRIBUTE_NO_SCRUB_DATA = 0x00020000;
        public const uint FILE_ATTRIBUTE_EA = 0x00040000;
        public const uint INVALID_ATTRIBUTE = 4294967295;
    }
    class CreateOption {

        public const uint FILE_DIRECTORY_FILE = 0x00000001;
        public const uint FILE_WRITE_THROUGH = 0x00000002;
        public const uint FILE_SEQUENTIAL_ONLY = 0x00000004;
        public const uint FILE_NO_INTERMEDIATE_BUFFERING = 0x00000008;

        public const uint FILE_SYNCHRONOUS_IO_ALERT = 0x00000010;
        public const uint FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020;
        public const uint FILE_NON_DIRECTORY_FILE = 0x00000040;
        public const uint FILE_CREATE_TREE_CONNECTION = 0x00000080;

        public const uint FILE_COMPLETE_IF_OPLOCKED = 0x00000100;
        public const uint FILE_NO_EA_KNOWLEDGE = 0x00000200;
        public const uint FILE_OPEN_REMOTE_INSTANCE = 0x00000400;
        public const uint FILE_RANDOM_ACCESS = 0x00000800;

        public const uint FILE_DELETE_ON_CLOSE = 0x00001000;
        public const uint FILE_OPEN_BY_FILE_ID = 0x00002000;
        public const uint FILE_OPEN_FOR_BACKUP_INTENT = 0x00004000;
        public const uint FILE_NO_COMPRESSION = 0x00008000;
    }
}
