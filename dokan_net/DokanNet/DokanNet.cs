using System;
using System.Collections.Generic;
using System.Text;
using System.Runtime.InteropServices;

namespace Dokan
{
    public class DokanOptions
    {
        public char DriveLetter;
        public ushort ThreadCount;
        public bool DebugMode;
        public bool UseStdErr;
        public bool UseAltStream;
        public bool UseKeepAlive;
        public string VolumeLabel;
    }


    // this struct must be the same layout as DOKAN_OPERATIONS
    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct DOKAN_OPERATIONS
    {
        public Proxy.CreateFileDelegate CreateFile;
        public Proxy.OpenDirectoryDelegate OpenDirectory;
        public Proxy.CreateDirectoryDelegate CreateDirectory;
        public Proxy.CleanupDelegate Cleanup;
        public Proxy.CloseFileDelegate CloseFile;
        public Proxy.ReadFileDelegate ReadFile;
        public Proxy.WriteFileDelegate WriteFile;
        public Proxy.FlushFileBuffersDelegate FlushFileBuffers;
        public Proxy.GetFileInformationDelegate GetFileInformation;
        public Proxy.FindFilesDelegate FindFiles;
        public IntPtr FindFilesWithPattern; // this is not used in DokanNet
        public Proxy.SetFileAttributesDelegate SetFileAttributes;
        public Proxy.SetFileTimeDelegate SetFileTime;
        public Proxy.DeleteFileDelegate DeleteFile;
        public Proxy.DeleteDirectoryDelegate DeleteDirectory;
        public Proxy.MoveFileDelegate MoveFile;
        public Proxy.SetEndOfFileDelegate SetEndOfFile;
        public Proxy.LockFileDelegate LockFile;
        public Proxy.UnlockFileDelegate UnlockFile;
        public Proxy.GetDiskFreeSpaceDelegate GetDiskFreeSpace;
        public Proxy.GetVolumeInformationDelegate GetVolumeInformation;
        public Proxy.UnmountDelegate Unmount;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 4)]
    struct DOKAN_OPTIONS
    {
        public char DriveLetter; // driver letter to be mounted
        public ushort ThreadCount; // number of threads to be used
        public byte DebugMode; // print debug message
        public byte UseStdErr; // output debug message to stderr
        public byte UseAltStream; // use alternate stream
        public byte UseKeepAlive; // use automacic unmount
        public ulong Dummy1;
    }


    class Dokan
    {
        [DllImport("dokan.dll")]
        public static extern int DokanMain(ref DOKAN_OPTIONS options, ref DOKAN_OPERATIONS operations);

        [DllImport("dokan.dll")]
        public static extern int DokanUnmount(int driveLetter);
    }


    public class DokanNet
    {
        public const int ERROR_FILE_NOT_FOUND       = 2;
        public const int ERROR_PATH_NOT_FOUND       = 3;
        public const int ERROR_ACCESS_DENIED        = 5;
        public const int ERROR_SHARING_VIOLATION    = 32;
        public const int ERROR_INVALID_NAME         = 123;
        public const int ERROR_FILE_EXISTS          = 80;
        public const int ERROR_ALREADY_EXISTS       = 183;

        public const int DOKAN_SUCCESS              = 0;
        public const int DOKAN_ERROR                = -1; // General Error
        public const int DOKAN_DRIVE_LETTER_ERROR   = -2; // Bad Drive letter
        public const int DOKAN_DRIVER_INSTALL_ERROR = -3; // Can't install driver
        public const int DOKAN_START_ERROR          = -4; // Driver something wrong
        public const int DOKAN_MOUNT_ERROR          = -5; // Can't assign drive letter


        public static int DokanMain(DokanOptions options, DokanOperations operations)
        {
            if (options.VolumeLabel == null)
            {
                options.VolumeLabel = "DOKAN";
            }
            
            Proxy proxy = new Proxy(options, operations);

            DOKAN_OPTIONS dokanOptions = new DOKAN_OPTIONS();

            dokanOptions.DriveLetter = options.DriveLetter;
            dokanOptions.ThreadCount = options.ThreadCount;
            dokanOptions.DebugMode = (byte)(options.DebugMode ? 1 : 0);
            dokanOptions.UseStdErr = (byte)(options.UseStdErr ? 1 : 0);
            dokanOptions.UseAltStream = (byte)(options.UseAltStream ? 1 : 0);
            dokanOptions.UseKeepAlive = (byte)(options.UseKeepAlive ? 1 : 0);

            DOKAN_OPERATIONS dokanOperations = new DOKAN_OPERATIONS();
            dokanOperations.CreateFile = proxy.CreateFileProxy;
            dokanOperations.OpenDirectory = proxy.OpenDirectoryProxy;
            dokanOperations.CreateDirectory = proxy.CreateDirectoryProxy;
            dokanOperations.Cleanup = proxy.CleanupProxy;
            dokanOperations.CloseFile = proxy.CloseFileProxy;
            dokanOperations.ReadFile = proxy.ReadFileProxy;
            dokanOperations.WriteFile = proxy.WriteFileProxy;
            dokanOperations.FlushFileBuffers = proxy.FlushFileBuffersProxy;
            dokanOperations.GetFileInformation = proxy.GetFileInformationProxy;
            dokanOperations.FindFiles = proxy.FindFilesProxy;
            dokanOperations.SetFileAttributes = proxy.SetFileAttributesProxy;
            dokanOperations.SetFileTime = proxy.SetFileTimeProxy;
            dokanOperations.DeleteFile = proxy.DeleteFileProxy;
            dokanOperations.DeleteDirectory = proxy.DeleteDirectoryProxy;
            dokanOperations.MoveFile = proxy.MoveFileProxy;
            dokanOperations.SetEndOfFile = proxy.SetEndOfFileProxy;
            dokanOperations.LockFile = proxy.LockFileProxy;
            dokanOperations.UnlockFile = proxy.UnlockFileProxy;
            dokanOperations.GetDiskFreeSpace = proxy.GetDiskFreeSpaceProxy;           
            dokanOperations.GetVolumeInformation = proxy.GetVolumeInformationProxy;        
            dokanOperations.Unmount = proxy.UnmountProxy;

            return Dokan.DokanMain(ref dokanOptions, ref dokanOperations);
        }


        public static int DokanUnmount(char driveLetter)
        {
            return Dokan.DokanUnmount(driveLetter);
        }
    }
}
