using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Runtime.InteropServices;

using ComTypes = System.Runtime.InteropServices.ComTypes;

namespace Dokan
{
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct BY_HANDLE_FILE_INFORMATION
    {
        public uint dwFileAttributes;
        public ComTypes.FILETIME ftCreationTime;
        public ComTypes.FILETIME ftLastAccessTime;
        public ComTypes.FILETIME ftLastWriteTime;
        public uint dwVolumeSerialNumber;
        public uint nFileSizeHigh;
        public uint nFileSizeLow;
        public uint dwNumberOfLinks;
        public uint nFileIndexHigh;
        public uint nFileIndexLow;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct DOKAN_FILE_INFO
    {
        public ulong Context;
        public ulong DokanContext;
        public uint ProcessId;
        public byte IsDirectory;
        public byte DeleteOnClose;
        public IntPtr Dummy;
    }


    class Proxy
    {
        private DokanOperations operations_;
        private ArrayList array_;
        private Dictionary<ulong, DokanFileInfo> infoTable_;
        private ulong infoId_ = 0;
        private object infoTableLock_ = new object();
        private DokanOptions options_;

        public Proxy(DokanOptions options, DokanOperations operations)
        {
            operations_ = operations;
            options_ = options;
            array_ = new ArrayList();
            infoTable_ = new Dictionary<ulong, DokanFileInfo>();
        }

        private DokanFileInfo GetNewFileInfo(ref DOKAN_FILE_INFO FileInfo)
        {
            DokanFileInfo info = new DokanFileInfo();

            lock (infoTableLock_)
            {
                info.InfoId = ++infoId_;

                FileInfo.Context = info.InfoId;
                info.IsDirectory = FileInfo.IsDirectory == 1 ? true : false;
                info.ProcessId = FileInfo.ProcessId;

                // to avoid GC
                infoTable_[info.InfoId] = info;
            }
            return info;
        }

        private DokanFileInfo GetFileInfo(ref DOKAN_FILE_INFO info)
        {
            DokanFileInfo fileinfo = null;
            lock (infoTableLock_)
            {
                if (info.Context != 0)
                {
                    infoTable_.TryGetValue(info.Context, out fileinfo);
                }

                if (fileinfo == null)
                {
                    // bug?
                    fileinfo = new DokanFileInfo();
                }

                fileinfo.IsDirectory = info.IsDirectory == 1 ? true : false;
                fileinfo.ProcessId = info.ProcessId;
                fileinfo.DeleteOnClose = info.DeleteOnClose == 1 ? true : false;
            }
            return fileinfo;
        }
      
        private string GetFileName(IntPtr FileName)
        {
            return Marshal.PtrToStringUni(FileName);
        }


        private const uint GENERIC_READ = 0x80000000;
        private const uint GENERIC_WRITE = 0x40000000;
        private const uint GENERIC_EXECUTE = 0x20000000;
        
        private const uint FILE_READ_DATA = 0x0001;
        private const uint FILE_READ_ATTRIBUTES = 0x0080;
        private const uint FILE_READ_EA = 0x0008;
        private const uint FILE_WRITE_DATA = 0x0002;
        private const uint FILE_WRITE_ATTRIBUTES = 0x0100;
        private const uint FILE_WRITE_EA = 0x0010;

        private const uint FILE_SHARE_READ = 0x00000001;
        private const uint FILE_SHARE_WRITE = 0x00000002;
        private const uint FILE_SHARE_DELETE = 0x00000004;

        private const uint CREATE_NEW = 1;
        private const uint CREATE_ALWAYS = 2;
        private const uint OPEN_EXISTING = 3;
        private const uint OPEN_ALWAYS = 4;
        private const uint TRUNCATE_EXISTING = 5;
        
        private const uint FILE_ATTRIBUTE_ARCHIVE = 0x00000020;
        private const uint FILE_ATTRIBUTE_ENCRYPTED = 0x00004000;
        private const uint FILE_ATTRIBUTE_HIDDEN = 0x00000002;
        private const uint FILE_ATTRIBUTE_NORMAL = 0x00000080;
        private const uint FILE_ATTRIBUTE_NOT_CONTENT_INDEXED = 0x00002000;
        private const uint FILE_ATTRIBUTE_OFFLINE = 0x00001000;
        private const uint FILE_ATTRIBUTE_READONLY = 0x00000001;
        private const uint FILE_ATTRIBUTE_SYSTEM = 0x00000004;
        private const uint FILE_ATTRIBUTE_TEMPORARY = 0x00000100;

        
        public delegate int CreateFileDelegate(
            IntPtr FileName,
            uint AccessMode,
            uint Share,
            uint CreationDisposition,
            uint FlagsAndAttributes,
            ref DOKAN_FILE_INFO DokanFileInfo);

        public int CreateFileProxy(
            IntPtr FileName,
            uint AccessMode,
            uint Share,
            uint CreationDisposition,
            uint FlagsAndAttributes,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {

                string file = GetFileName(FileName);

                DokanFileInfo info = GetNewFileInfo(ref FileInfo);
                
                FileAccess access = FileAccess.Read;
                FileShare share = FileShare.None;
                FileMode mode = FileMode.Open;
                FileOptions options = FileOptions.None;

                if ((AccessMode & FILE_READ_DATA) != 0 && (AccessMode & FILE_WRITE_DATA) != 0)
                {
                    access = FileAccess.ReadWrite;
                }
                else if ((AccessMode & FILE_WRITE_DATA) != 0)
                {
                    access = FileAccess.Write;
                }
                else
                {
                    access = FileAccess.Read;
                }

                if ((Share & FILE_SHARE_READ) != 0)
                {
                    share = FileShare.Read;
                }

                if ((Share & FILE_SHARE_WRITE) != 0)
                {
                    share |= FileShare.Write;
                }

                if ((Share & FILE_SHARE_DELETE) != 0)
                {
                    share |= FileShare.Delete;
                }

                switch (CreationDisposition)
                {
                    case CREATE_NEW:
                        mode = FileMode.CreateNew;
                        break;
                    case CREATE_ALWAYS:
                        mode = FileMode.Create;
                        break;
                    case OPEN_EXISTING:
                        mode = FileMode.Open;
                        break;
                    case OPEN_ALWAYS:
                        mode = FileMode.OpenOrCreate;
                        break;
                    case TRUNCATE_EXISTING:
                        mode = FileMode.Truncate;
                        break;
                }

                int ret = operations_.CreateFile(file, access, share, mode, options, info);

                if (info.IsDirectory)
                    FileInfo.IsDirectory = 1;

                return ret;
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -2;
            }

        }

        ////

        public delegate int OpenDirectoryDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int OpenDirectoryProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                DokanFileInfo info = GetNewFileInfo(ref FileInfo);
                return operations_.OpenDirectory(file, info);

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int CreateDirectoryDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int CreateDirectoryProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                DokanFileInfo info = GetNewFileInfo(ref FileInfo);
                return operations_.CreateDirectory(file, info);

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int CleanupDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int CleanupProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                return operations_.Cleanup(file, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int CloseFileDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int CloseFileProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                DokanFileInfo info = GetFileInfo(ref FileInfo);

                int ret = operations_.CloseFile(file, info);

                FileInfo.Context = 0;
                infoTable_.Remove(info.InfoId);

                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int ReadFileDelegate(
            IntPtr FileName,
            IntPtr Buffer,
            uint BufferLength,
            ref uint ReadLength,
            long Offset,
            ref DOKAN_FILE_INFO FileInfo);

        public int ReadFileProxy(
            IntPtr FileName,
            IntPtr Buffer,
            uint BufferLength,
            ref uint ReadLength,
            long Offset,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                byte[] buf = new Byte[BufferLength];

                uint readLength = 0;
                int ret = operations_.ReadFile(
                    file, buf, ref readLength, Offset, GetFileInfo(ref FileInfo));
                if (ret == 0)
                {
                    ReadLength = readLength;
                    Marshal.Copy(buf, 0, Buffer, (int)BufferLength);
                }
                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int WriteFileDelegate(
            IntPtr FileName,
            IntPtr Buffer,
            uint NumberOfBytesToWrite,
            ref uint NumberOfBytesWritten,
            long Offset,
            ref DOKAN_FILE_INFO FileInfo);

        public int WriteFileProxy(
            IntPtr FileName,
            IntPtr Buffer,
            uint NumberOfBytesToWrite,
            ref uint NumberOfBytesWritten,
            long Offset,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                Byte[] buf = new Byte[NumberOfBytesToWrite];
                Marshal.Copy(Buffer, buf, 0, (int)NumberOfBytesToWrite);

                uint bytesWritten = 0;
                int ret = operations_.WriteFile(
                    file, buf, ref bytesWritten, Offset, GetFileInfo(ref FileInfo));
                if (ret == 0)
                    NumberOfBytesWritten = bytesWritten;
                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int FlushFileBuffersDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int FlushFileBuffersProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                int ret = operations_.FlushFileBuffers(file, GetFileInfo(ref FileInfo));
                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }
       
        ////

        public delegate int GetFileInformationDelegate(
            IntPtr FileName,
            ref BY_HANDLE_FILE_INFORMATION HandleFileInfo,
            ref DOKAN_FILE_INFO FileInfo);

        public int GetFileInformationProxy(
            IntPtr FileName,
            ref BY_HANDLE_FILE_INFORMATION HandleFileInformation,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                FileInformation fi = new FileInformation();

                int ret = operations_.GetFileInformation(file, fi, GetFileInfo(ref FileInfo));

                if (ret == 0)
                {
                    HandleFileInformation.dwFileAttributes = (uint)fi.Attributes;

                    HandleFileInformation.ftCreationTime.dwHighDateTime =
                        (int)(fi.CreationTime.ToFileTime() >> 32);
                    HandleFileInformation.ftCreationTime.dwLowDateTime =
                        (int)(fi.CreationTime.ToFileTime() & 0xffffffff);

                    HandleFileInformation.ftLastAccessTime.dwHighDateTime =
                        (int)(fi.LastAccessTime.ToFileTime() >> 32);
                    HandleFileInformation.ftLastAccessTime.dwLowDateTime =
                        (int)(fi.LastAccessTime.ToFileTime() & 0xffffffff);

                    HandleFileInformation.ftLastWriteTime.dwHighDateTime =
                        (int)(fi.LastWriteTime.ToFileTime() >> 32);
                    HandleFileInformation.ftLastWriteTime.dwLowDateTime =
                        (int)(fi.LastWriteTime.ToFileTime() & 0xffffffff);

                    HandleFileInformation.nFileSizeLow =
                        (uint)(fi.Length & 0xffffffff);
                    HandleFileInformation.nFileSizeHigh =
                        (uint)(fi.Length >> 32);
                }

                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }

        }

        ////

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Auto, Pack = 4)]
        struct WIN32_FIND_DATA
        {
            public FileAttributes dwFileAttributes;
            public ComTypes.FILETIME ftCreationTime;
            public ComTypes.FILETIME ftLastAccessTime;
            public ComTypes.FILETIME ftLastWriteTime;
            public uint nFileSizeHigh;
            public uint nFileSizeLow;
            public uint dwReserved0;
            public uint dwReserved1;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)]
            public string cFileName;
            [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 14)]
            public string cAlternateFileName;
        }

        private delegate int FILL_FIND_DATA(
            ref WIN32_FIND_DATA FindData,
            ref DOKAN_FILE_INFO FileInfo);

        public delegate int FindFilesDelegate(
            IntPtr FileName,
            IntPtr FillFindData, // function pointer
            ref DOKAN_FILE_INFO FileInfo);

        public int FindFilesProxy(
            IntPtr FileName,
            IntPtr FillFindData, // function pointer
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                ArrayList files = new ArrayList();
                int ret = operations_.FindFiles(file, files, GetFileInfo(ref FileInfo));

                FILL_FIND_DATA fill = (FILL_FIND_DATA)Marshal.GetDelegateForFunctionPointer(
                    FillFindData, typeof(FILL_FIND_DATA));

                if (ret == 0)
                {
                    IEnumerator entry = files.GetEnumerator();
                    while (entry.MoveNext())
                    {
                        FileInformation fi = (FileInformation)(entry.Current);
                        WIN32_FIND_DATA data = new WIN32_FIND_DATA();
                        //ZeroMemory(&data, sizeof(WIN32_FIND_DATAW));

                        data.dwFileAttributes = fi.Attributes;

                        data.ftCreationTime.dwHighDateTime =
                            (int)(fi.CreationTime.ToFileTime() >> 32);
                        data.ftCreationTime.dwLowDateTime =
                            (int)(fi.CreationTime.ToFileTime() & 0xffffffff);

                        data.ftLastAccessTime.dwHighDateTime =
                            (int)(fi.LastAccessTime.ToFileTime() >> 32);
                        data.ftLastAccessTime.dwLowDateTime =
                            (int)(fi.LastAccessTime.ToFileTime() & 0xffffffff);

                        data.ftLastWriteTime.dwHighDateTime =
                            (int)(fi.LastWriteTime.ToFileTime() >> 32);
                        data.ftLastWriteTime.dwLowDateTime =
                            (int)(fi.LastWriteTime.ToFileTime() & 0xffffffff);

                        data.nFileSizeLow =
                            (uint)(fi.Length & 0xffffffff);
                        data.nFileSizeHigh =
                            (uint)(fi.Length >> 32);

                        data.cFileName = fi.FileName;

                        fill(ref data, ref FileInfo);
                    }

                }
                return ret;

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }

        }

        ////

        public delegate int SetEndOfFileDelegate(
            IntPtr FileName,
            long ByteOffset,
            ref DOKAN_FILE_INFO FileInfo);

        public int SetEndOfFileProxy(
            IntPtr FileName,
            long ByteOffset,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                return operations_.SetEndOfFile(file, ByteOffset, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {

                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }


        public delegate int SetAllocationSizeDelegate(
            IntPtr FileName,
            long Length,
            ref DOKAN_FILE_INFO FileInfo);

        public int SetAllocationSizeProxy(
            IntPtr FileName,
            long Length,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                return operations_.SetAllocationSize(file, Length, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {

                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }


      ////

        public delegate int SetFileAttributesDelegate(
            IntPtr FileName,
            uint Attributes,
            ref DOKAN_FILE_INFO FileInfo);

        public int SetFileAttributesProxy(
            IntPtr FileName,
            uint Attributes,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                FileAttributes attr = (FileAttributes)Attributes;
                return operations_.SetFileAttributes(file, attr, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int SetFileTimeDelegate(
            IntPtr FileName,
            ref ComTypes.FILETIME CreationTime,
            ref ComTypes.FILETIME LastAccessTime,
            ref ComTypes.FILETIME LastWriteTime,
            ref DOKAN_FILE_INFO FileInfo);

        public int SetFileTimeProxy(
            IntPtr FileName,
            ref ComTypes.FILETIME CreationTime,
            ref ComTypes.FILETIME LastAccessTime,
            ref ComTypes.FILETIME LastWriteTime,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                long time;

                time = ((long)CreationTime.dwHighDateTime << 32) + (uint)CreationTime.dwLowDateTime;
                DateTime ctime = DateTime.FromFileTime(time);
                
                if (time == 0)
                    ctime = DateTime.MinValue;

                time = ((long)LastAccessTime.dwHighDateTime << 32) + (uint)LastAccessTime.dwLowDateTime;
                DateTime atime = DateTime.FromFileTime(time);

                if (time == 0)
                    atime = DateTime.MinValue;

                time = ((long)LastWriteTime.dwHighDateTime << 32) + (uint)LastWriteTime.dwLowDateTime;
                DateTime mtime = DateTime.FromFileTime(time);

                if (time == 0)
                    mtime = DateTime.MinValue;

                return operations_.SetFileTime(
                    file, ctime, atime, mtime, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int DeleteFileDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int DeleteFileProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);

                return operations_.DeleteFile(file, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int DeleteDirectoryDelegate(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo);

        public int DeleteDirectoryProxy(
            IntPtr FileName,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                return operations_.DeleteDirectory(file, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

       ////

        public delegate int MoveFileDelegate(
            IntPtr FileName,
            IntPtr NewFileName,
            int ReplaceIfExisting,
            ref DOKAN_FILE_INFO FileInfo);

        public int MoveFileProxy(
            IntPtr FileName,
            IntPtr NewFileName,
            int ReplaceIfExisting,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                string newfile = GetFileName(NewFileName);

                return operations_.MoveFile(
                    file, newfile, ReplaceIfExisting != 0 ? true : false,
                    GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int LockFileDelegate(
            IntPtr FileName,
            long ByteOffset,
            long Length,
            ref DOKAN_FILE_INFO FileInfo);

        public int LockFileProxy(
            IntPtr FileName,
            long ByteOffset,
            long Length,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                return operations_.LockFile(
                    file, ByteOffset, Length, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

       ////

        public delegate int UnlockFileDelegate(
            IntPtr FileName,
            long ByteOffset,
            long Length,
            ref DOKAN_FILE_INFO FileInfo);

        public int UnlockFileProxy(
            IntPtr FileName,
            long ByteOffset,
            long Length,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                string file = GetFileName(FileName);
                return operations_.UnlockFile(
                    file, ByteOffset, Length, GetFileInfo(ref FileInfo));

            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        ////

        public delegate int GetDiskFreeSpaceDelegate(
            ref ulong FreeBytesAvailable,
            ref ulong TotalNumberOfBytes,
            ref ulong TotalNumberOfFreeBytes,
            ref DOKAN_FILE_INFO FileInfo);

        public int GetDiskFreeSpaceProxy(
            ref ulong FreeBytesAvailable,
            ref ulong TotalNumberOfBytes,
            ref ulong TotalNumberOfFreeBytes,
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                return operations_.GetDiskFreeSpace(
                    ref FreeBytesAvailable,
                    ref TotalNumberOfBytes,
                    ref TotalNumberOfFreeBytes,
                    GetFileInfo(ref FileInfo));
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }

        public delegate int GetVolumeInformationDelegate(
            IntPtr VolumeNameBuffer,
            uint VolumeNameSize,
            ref uint VolumeSerialNumber,
            ref uint MaximumComponentLength,
            ref uint FileSystemFlags,
            IntPtr FileSystemNameBuffer,
            uint FileSystemNameSize,
            ref DOKAN_FILE_INFO FileInfo);

        public int GetVolumeInformationProxy(
            IntPtr VolumeNameBuffer,
            uint VolumeNameSize,
            ref uint VolumeSerialNumber,
            ref uint MaximumComponentLength,
            ref uint FileSystemFlags,
            IntPtr FileSystemNameBuffer,
            uint FileSystemNameSize,
            ref DOKAN_FILE_INFO FileInfo)
        {
            byte[] volume = System.Text.Encoding.Unicode.GetBytes(options_.VolumeLabel);
            Marshal.Copy(volume, 0, VolumeNameBuffer, Math.Min((int)VolumeNameSize, volume.Length));
            VolumeSerialNumber = 0x19831116;
            MaximumComponentLength = 256;

            // FILE_CASE_SENSITIVE_SEARCH | 
            // FILE_CASE_PRESERVED_NAMES |
            // FILE_UNICODE_ON_DISK
            FileSystemFlags = 7;

            byte[] sys = System.Text.Encoding.Unicode.GetBytes("DOKAN");
            Marshal.Copy(sys, 0, FileSystemNameBuffer, Math.Min((int)FileSystemNameSize, sys.Length));
            return 0;
        }


        public delegate int UnmountDelegate(
            ref DOKAN_FILE_INFO FileInfo);

        public int UnmountProxy(
            ref DOKAN_FILE_INFO FileInfo)
        {
            try
            {
                return operations_.Unmount(GetFileInfo(ref FileInfo));
            }
            catch (Exception e)
            {
                Console.Error.WriteLine(e.ToString());
                return -1;
            }
        }
    }
}
