#ifndef __TABBASE_H__
#define __TABBASE_H__

#include <QtCore>
#include <QtWidgets>
#include "initialshoweventmixin.h"

class PrintJob;
class PrintManager;
class PrintProfileManager;
class Shepherd;
class UsbMountManager;
class OrderManifestManager;

class TabBase: public QWidget {

    Q_OBJECT;
    Q_PROPERTY( QSharedPointer<PrintJob>  printJob       READ printJob            WRITE setPrintJob            );
    Q_PROPERTY( PrintManager*        printManager        READ printManager        WRITE setPrintManager        );
    Q_PROPERTY( PrintProfileManager* printProfileManager READ printProfileManager WRITE setPrintProfileManager );
    Q_PROPERTY( Shepherd*            shepherd            READ shepherd            WRITE setShepherd            );
    Q_PROPERTY( UsbMountManager*     usbMountManager     READ usbMountManager     WRITE setUsbMountManager     );
    Q_PROPERTY( TabIndex             tabIndex            READ tabIndex                                         );
    Q_PROPERTY( UiState              uiState             READ uiState                                          );

public:

    enum class TabIndex {
        File,
        Prepare,
        Tiling,
        Print,
        Status,
        Advanced,
        Profiles,
        System,
    };
    Q_ENUM( TabIndex );

    enum class UiState {
        SelectStarted,
        SelectCompleted,
        SliceStarted,
        SliceCompleted,
        PrintJobReady,
        PrintStarted,
        PrintCompleted,
        TilingClicked
    };
    Q_ENUM( UiState );

    TabBase( QWidget* parent = nullptr );
    virtual ~TabBase( ) override;

    QSharedPointer<PrintJob> printJob( )        const { return _printJob;            }
    PrintManager*        printManager( )        const { return _printManager;        }
    PrintProfileManager* printProfileManager( ) const { return _printProfileManager; }
    Shepherd*            shepherd( )            const { return _shepherd;            }
    UiState              uiState( )             const { return _uiState;             }
    UsbMountManager*     usbMountManager( )     const { return _usbMountManager;     }

    virtual TabIndex tabIndex( )        const = 0;

protected:

    QSharedPointer<PrintJob> _printJob                { };
    PrintManager*           _printManager             { };
    PrintProfileManager*    _printProfileManager      { };
    Shepherd*               _shepherd                 { };
    UiState                 _uiState                  { };
    UsbMountManager*        _usbMountManager          { };

    virtual void _disconnectPrintJob( );
    virtual void _connectPrintJob( );

    virtual void _disconnectPrintManager( );
    virtual void _connectPrintManager( );

    virtual void _disconnectPrintProfileManager( );
    virtual void _connectPrintProfileManager( );

    virtual void _disconnectShepherd( );
    virtual void _connectShepherd( );

    virtual void _disconnectUsbMountManager( );
    virtual void _connectUsbMountManager( );

private:

signals:
    ;

    void uiStateChanged( TabIndex const sender, UiState const state );
    void iconChanged( TabIndex const sender, QIcon const& icon );

public slots:
    virtual void setPrintJob( QSharedPointer<PrintJob> printJob );
    virtual void setPrintManager( PrintManager* printManager );
    virtual void setPrintProfileManager( PrintProfileManager* printProfileManager );
    virtual void setShepherd( Shepherd* shepherd );
    virtual void setUsbMountManager( UsbMountManager* mountManager );
    virtual void tab_uiStateChanged( TabIndex const sender, UiState const state ) = 0;

protected slots:
    ;

private slots:
    ;

};

using TabIndex = TabBase::TabIndex;
using UiState  = TabBase::UiState;

inline int operator+( TabIndex const value ) { return static_cast<int>( value ); }
inline int operator+( UiState  const value ) { return static_cast<int>( value ); }

char const* ToString( TabIndex const value );
char const* ToString( UiState  const value );

#endif // !__TABBASE_H__
