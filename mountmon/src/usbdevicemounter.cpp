#include "pch.h"

#include "usbdevicemounter.h"

#include "processrunner.h"
#include "signalhandler.h"
#include "udisksmonitor.h"

extern uid_t LightFieldUserId;
extern gid_t LightFieldGroupId;

namespace {

    char const* DriveSizeUnits[] = {
        "iB",
        "KiB",
        "MiB",
        "GiB",
        "TiB",
        "PiB",
        "EiB",
        "ZiB",
        "YiB"
    };

    std::initializer_list<int> SignalList {
        SIGHUP,
        SIGINT,
        SIGPIPE,
        SIGQUIT,
        SIGTERM,
    };

    QRegularExpression const NewLineRegex        { "\\r?\\n" };
    QChar              const LineFeed            { '\n'      };
    std::atomic_int          NextMountPointIndex { 0         };

    void ScaleSize( qulonglong const inputSize, double& scaledSize, char const*& suffix ) {
        int unitIndex = 0;

        scaledSize = inputSize;
        while ( scaledSize > 1024.0 ) {
            ++unitIndex;
            scaledSize /= 1024.0;
        }
        suffix = DriveSizeUnits[unitIndex];
    }

    QString ToString( UDrive* drive ) {
        char const* unit;
        double driveSize;

        ScaleSize( drive->Size, driveSize, unit );
        return QString::asprintf(
            "  + Path:                  %s\n"
            "  + CanPowerOff:           %s\n"
            "  + ConnectionBus:         %s\n"
            "  + Ejectable:             %s\n"
            "  + Id:                    %s\n"
            "  + Media:                 %s\n"
            "  + MediaAvailable:        %s\n"
            "  + MediaChangeDetected:   %s\n"
            "  + MediaRemovable:        %s\n"
            "  + Model:                 %s\n"
            "  + Optical:               %s\n"
            "  + OpticalBlank:          %s\n"
            "  + OpticalNumAudioTracks: %u\n"
            "  + OpticalNumDataTracks:  %u\n"
            "  + OpticalNumSessions:    %u\n"
            "  + OpticalNumTracks:      %u\n"
            "  + Removable:             %s\n"
            "  + Revision:              %s\n"
            "  + RotationRate:          %d\n"
            "  + Seat:                  %s\n"
            "  + Serial:                %s\n"
            "  + SiblingId:             %s\n"
            "  + Size:                  %llu (%.2f %s)\n"
            "  + SortKey:               %s\n"
            "  + TimeDetected:          %llu\n"
            "  + TimeMediaDetected:     %llu\n"
            "  + Vendor:                %s\n"
            "  + WWN:                   %s\n"
            "",
            drive->Path.path( ).toUtf8( ).data( ),
            drive->CanPowerOff ? "true" : "false",
            drive->ConnectionBus.toUtf8( ).data( ),
            drive->Ejectable ? "true" : "false",
            drive->Id.toUtf8( ).data( ),
            drive->Media.toUtf8( ).data( ),
            drive->MediaAvailable ? "true" : "false",
            drive->MediaChangeDetected ? "true" : "false",
            drive->MediaRemovable ? "true" : "false",
            drive->Model.toUtf8( ).data( ),
            drive->Optical ? "true" : "false",
            drive->OpticalBlank ? "true" : "false",
            drive->OpticalNumAudioTracks,
            drive->OpticalNumDataTracks,
            drive->OpticalNumSessions,
            drive->OpticalNumTracks,
            drive->Removable ? "true" : "false",
            drive->Revision.toUtf8( ).data( ),
            drive->RotationRate,
            drive->Seat.toUtf8( ).data( ),
            drive->Serial.toUtf8( ).data( ),
            drive->SiblingId.toUtf8( ).data( ),
            drive->Size, driveSize, unit,
            drive->SortKey.toUtf8( ).data( ),
            drive->TimeDetected,
            drive->TimeMediaDetected,
            drive->Vendor.toUtf8( ).data( ),
            drive->WWN.toUtf8( ).data( )
        );
    }

}

UsbDeviceMounter::UsbDeviceMounter( UDisksMonitor* monitor, QObject* parent ):
    QObject  ( parent  ),
    _monitor ( monitor )
{
    _signalHandler = new SignalHandler { this };

    (void) QObject::connect( _monitor,       &UDisksMonitor::driveAdded,         this, &UsbDeviceMounter::_driveAdded         );
    (void) QObject::connect( _monitor,       &UDisksMonitor::filesystemAdded,    this, &UsbDeviceMounter::_filesystemAdded    );
    (void) QObject::connect( _monitor,       &UDisksMonitor::blockDeviceAdded,   this, &UsbDeviceMounter::_blockDeviceAdded   );

    (void) QObject::connect( _monitor,       &UDisksMonitor::driveRemoved,       this, &UsbDeviceMounter::_driveRemoved       );
    (void) QObject::connect( _monitor,       &UDisksMonitor::filesystemRemoved,  this, &UsbDeviceMounter::_filesystemRemoved  );
    (void) QObject::connect( _monitor,       &UDisksMonitor::blockDeviceRemoved, this, &UsbDeviceMounter::_blockDeviceRemoved );

    (void) QObject::connect( _monitor,       &UDisksMonitor::ready,              this, &UsbDeviceMounter::_monitorReady       );

    (void) QObject::connect( _signalHandler, &SignalHandler::signalReceived,     this, &UsbDeviceMounter::_signalReceived     );

    _signalHandler->subscribe( SignalList );

    ::mkdir( "/media/lumen", 0755 );
    ::chown( "/media/lumen", LightFieldUserId, LightFieldGroupId );

    monitor->probeForExistingDevices( );
}

UsbDeviceMounter::~UsbDeviceMounter( ) {
    QObject::disconnect( _monitor );
}

void UsbDeviceMounter::_dumpStdioBuffers( ) {
    if ( !_mountStderrBuffer.isEmpty( ) ) {
        if ( _mountStderrBuffer.endsWith( LineFeed ) ) {
            _mountStderrBuffer.chop( 1 );
        }
        debug( "[mount/stderr] %s\n", _mountStderrBuffer.replace( NewLineRegex, "\n[mount/stderr] " ).toUtf8( ).data( ) );
        _mountStderrBuffer.clear( );
    }
    if ( !_mountStdoutBuffer.isEmpty( ) ) {
        if ( _mountStdoutBuffer.endsWith( LineFeed ) ) {
            _mountStdoutBuffer.chop( 1 );
        }
        debug( "[mount/stdout] %s\n", _mountStdoutBuffer.replace( NewLineRegex, "\n[mount/stdout] " ).toUtf8( ).data( ) );
        _mountStdoutBuffer.clear( );
    }
}

void UsbDeviceMounter::_mount( QDBusObjectPath const& path, UFilesystem* filesystem ) {
    debug( "+ UsbDeviceMounter::_mount: attempting to mount filesystem at D-Bus object path '%s'\n", path.path( ).toUtf8( ).data( ) );

    if ( !_blockDevices.contains( path ) ) {
        debug( "+ UsbDeviceMounter::_mount: can't find block device for D-Bus object path\n" );
        --_pendingMounts;
        _monitorReady( );
        return;
    }
    UBlockDevice* blockDevice = _blockDevices[path];

    QString newMountPoint;
    while ( true ) {
        newMountPoint = QString { "/media/lumen/%1" }.arg( NextMountPointIndex++ );

        QDir newMountPointDir { newMountPoint };
        if ( -1 == ::mkdir( newMountPoint.toUtf8( ).data( ), 0755 ) ) {
            continue;
        }
        ::chown( newMountPoint.toUtf8( ).data( ), LightFieldUserId, LightFieldGroupId );

        break;
    }

    debug( "+ UsbDeviceMounter::_mount: starting `/bin/mount` to mount '%s' read-only\n", blockDevice->Device.data( ) );
    auto mountProcess { new ProcessRunner };

    (void) QObject::connect( mountProcess, &ProcessRunner::succeeded, this, [ this, path, filesystem, newMountPoint, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_mount: mount of '%s' succeeded\n", newMountPoint.toUtf8( ).data( ) );

        filesystem->IsReadWrite   = false;
        filesystem->OurMountPoint = newMountPoint;

        _objectPathsToMountPoints.insert( path, newMountPoint );
        _mountPointsToObjectPaths.insert( newMountPoint, path );

        printf( "mount:%s\n", filesystem->OurMountPoint.toUtf8( ).data( ) );

        --_pendingMounts;
        _monitorReady( );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::failed, this, [ this, filesystem, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_mount: mount of %s failed\n", filesystem->OurMountPoint.toUtf8( ).data( ) );

        --_pendingMounts;
        _monitorReady( );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardOutput, this, &UsbDeviceMounter::_mount_readyReadStandardOutput );
    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardError,  this, &UsbDeviceMounter::_mount_readyReadStandardError  );

    mountProcess->start(
        QString { "/bin/mount" },
        QStringList {
            "-o",
            QString { "ro,nosuid,nodev,noexec,relatime,fmask=0133,dmask=0022,allow_utime=0020,codepage=437,iocharset=utf8,utf8,flush,errors=remount-ro,uid=%1,gid=%2" }.arg( LightFieldUserId ).arg( LightFieldGroupId ),
            blockDevice->Device,
            newMountPoint
        }
    );
}

void UsbDeviceMounter::_remount( QString const& path, bool const writable ) {
    auto iter = std::find_if( _filesystems.begin( ), _filesystems.end( ), [ path ] ( UFilesystem* item ) { return item->OurMountPoint == path; } );
    if ( iter == _filesystems.end( ) ) {
        debug( "+ UsbDeviceMounter::_remount: couldn't find mount point '%s' in our list of mounted filesystems, returning failure\n", path.toUtf8( ).data( ) );
        printf( "remount:failure:r%c:%s\n", writable ? 'w' : 'o', path.toUtf8( ).data( ) );
        return;
    }
    UFilesystem* filesystem = *iter;
    if ( filesystem->IsReadWrite == writable ) {
        debug( "+ UsbDeviceMounter::_remount: already mounted read-%s, returning success\n", writable ? "write" : "only" );
        printf( "remount:success:r%c:%s\n", writable ? 'w' : 'o', path.toUtf8( ).data( ) );
        return;
    }

    debug( "+ UsbDeviceMounter::_remount: starting `/bin/mount` to remount mount point '%s' read-%s\n", path.toUtf8( ).data( ), writable ? "write" : "only" );
    auto mountProcess { new ProcessRunner };

    (void) QObject::connect( mountProcess, &ProcessRunner::succeeded, this, [ this, filesystem, writable, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_remount: remount read-%s of %s succeeded\n", writable ? "write" : "only", filesystem->OurMountPoint.toUtf8( ).data( ) );

        filesystem->IsReadWrite = writable;

        printf( "remount:success:r%c:%s\n", writable ? 'w' : 'o', filesystem->OurMountPoint.toUtf8( ).data( ) );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::failed, this, [ this, filesystem, writable, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_remount: remount read-%s of %s failed\n", writable ? "write" : "only", filesystem->OurMountPoint.toUtf8( ).data( ) );

        printf( "remount:failure:r%c:%s\n", writable ? 'w' : 'o', filesystem->OurMountPoint.toUtf8( ).data( ) );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardOutput, this, &UsbDeviceMounter::_mount_readyReadStandardOutput );
    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardError,  this, &UsbDeviceMounter::_mount_readyReadStandardError  );

    mountProcess->start(
        QString { "/bin/mount" },
        QStringList {
            "-o",
            "remount,r" % QChar { writable ? 'w' : 'o' },
            path
        }
    );
}

void UsbDeviceMounter::_unmount( UFilesystem* filesystem ) {
    debug( "+ UsbDeviceMounter::_unmount: starting `/bin/umount` to unmount '%s'\n", filesystem->OurMountPoint.toUtf8( ).data( ) );
    auto mountProcess { new ProcessRunner };

    (void) QObject::connect( mountProcess, &ProcessRunner::succeeded, this, [ this, filesystem, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_unmount: unmount of '%s' succeeded\n", filesystem->OurMountPoint.toUtf8( ).data( ) );

        if ( -1 == ::rmdir( filesystem->OurMountPoint.toUtf8( ).data( ) ) ) {
            error_t err = errno;
            debug( "+ UsbDeviceMounter::_filesystemRemoved: couldn't delete now-unused mount point directory '%s': %s [%d]\n", filesystem->OurMountPoint.toUtf8( ).data( ), strerror( err ), err );
        }

        printf( "unmount:%s\n", filesystem->OurMountPoint.toUtf8( ).data( ) );

        auto path = _mountPointsToObjectPaths[filesystem->OurMountPoint];
        _objectPathsToMountPoints.remove( path );
        _mountPointsToObjectPaths.remove( filesystem->OurMountPoint );
        _filesystems.remove( path );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::failed, this, [ this, filesystem, mountProcess ] ( ) {
        _dumpStdioBuffers( );
        debug( "+ UsbDeviceMounter::_unmount: unmount of '%s' failed\n", filesystem->OurMountPoint.toUtf8( ).data( ) );

        mountProcess->deleteLater( );
    } );

    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardOutput, this, &UsbDeviceMounter::_mount_readyReadStandardOutput );
    (void) QObject::connect( mountProcess, &ProcessRunner::readyReadStandardError,  this, &UsbDeviceMounter::_mount_readyReadStandardError  );

    mountProcess->start(
        QString { "/bin/umount" },
        QStringList {
            filesystem->OurMountPoint
        }
    );
}

void UsbDeviceMounter::_mount_readyReadStandardOutput( QString const& data ) {
    _mountStdoutBuffer += data;

    auto index = _mountStdoutBuffer.lastIndexOf( '\n' );
    if ( -1 == index ) {
        return;
    }

    debug( "[mount/stdout] %s\n", _mountStdoutBuffer.left( index ).replace( NewLineRegex, "\n[mount/stdout] " ).toUtf8( ).data( ) );
    _mountStdoutBuffer.remove( 0, index + 1 );
}

void UsbDeviceMounter::_mount_readyReadStandardError( QString const& data ) {
    _mountStderrBuffer += data;

    auto index = _mountStderrBuffer.lastIndexOf( '\n' );
    if ( -1 == index ) {
        return;
    }

    debug( "[mount/stderr] %s\n", _mountStderrBuffer.left( index ).replace( NewLineRegex, "\n[mount/stderr] " ).toUtf8( ).data( ) );
    _mountStderrBuffer.remove( 0, index + 1 );
}

void UsbDeviceMounter::_monitorReady( ) {
    if ( _ready ) {
        return;
    }

    int pendingMounts = _pendingMounts;
    debug( "+ UsbDeviceMounter::_monitorReady: pending mounts: %d\n", pendingMounts );
    if ( 0 == _pendingMounts ) {
        _ready = true;
        printf( "ready:\n" );
    }
}

void UsbDeviceMounter::_driveAdded( QDBusObjectPath const& path, UDrive* drive ) {
    if ( ( 0 != drive->ConnectionBus.compare( "usb", Qt::CaseInsensitive ) ) ) {
        return;
    }

    debug( "+ UsbDeviceMounter::_driveAdded:\n%s", ToString( drive ).toUtf8( ).data( ) );

    drive->setParent( this );
    _drives.insert( path, drive );
}

void UsbDeviceMounter::_blockDeviceAdded( QDBusObjectPath const& path, UBlockDevice* blockDevice ) {
    if ( blockDevice->HintIgnore || !_drives.contains( blockDevice->Drive ) ) {
        return;
    }

    char const* unit;
    double driveSize;
    ScaleSize( blockDevice->Size, driveSize, unit );
    debug(
        "+ UsbDeviceMounter::_blockDeviceAdded:\n"
        "  + Path:   '%s'\n"
        "  + Device: '%s'\n"
        "  + Size:   %.2f %s\n"
        "  + System? %s\n"
        "",
        path.path( ).toUtf8( ).data( ),
        blockDevice->Device.data( ),
        driveSize, unit,
        blockDevice->HintSystem ? "yes" : "no"
    );

    blockDevice->setParent( this );
    _blockDevices.insert( path, blockDevice );
}

void UsbDeviceMounter::_filesystemAdded( QDBusObjectPath const& path, UFilesystem* filesystem ) {
    if ( !_blockDevices.contains( path ) ) {
        return;
    }

    debug( "+ UsbDeviceMounter::_filesystemAdded: path %s\n", path.path( ).toUtf8( ).data( ) );

    filesystem->setParent( this );
    _filesystems.insert( path, filesystem );

    ++_pendingMounts;
    _mount( path, filesystem );
}

void UsbDeviceMounter::_driveRemoved( QDBusObjectPath const& path ) {
    if ( _drives.remove( path ) > 0 ) {
        debug( "+ UsbDeviceMounter::_driveRemoved: path %s\n", path.path( ).toUtf8( ).data( ) );
    }
}

void UsbDeviceMounter::_blockDeviceRemoved( QDBusObjectPath const& path ) {
    if ( _blockDevices.remove( path ) > 0 ) {
        debug( "+ UsbDeviceMounter::_blockDeviceRemoved: path %s\n", path.path( ).toUtf8( ).data( ) );
    }
}

void UsbDeviceMounter::_filesystemRemoved( QDBusObjectPath const& path ) {
    if ( _filesystems.contains( path ) ) {
        debug( "+ UsbDeviceMounter::_filesystemRemoved: path %s\n", path.path( ).toUtf8( ).data( ) );
        _unmount( _filesystems[path] );
    }
}

void UsbDeviceMounter::_signalReceived( siginfo_t const& info ) {
    debug( "+ UsbDeviceMounter::_signalReceived: signal %s [%d]\n", ::strsignal( info.si_signo ), info.si_signo );
    ::exit( 0 );
}

void UsbDeviceMounter::commandReader_commandReceived( QStringList const& command ) {
    if ( command[0] == "terminate" ) {
        ::exit( 0 );
    } else if ( command[0] == "remount-ro" ) {
        _remount( command[1], false );
    } else if ( command[0] == "remount-rw" ) {
        _remount( command[1], true );
    } else {
        printf( "error:%s:Unrecognized command\n", command[0].toUtf8( ).data( ) );
    }
}
