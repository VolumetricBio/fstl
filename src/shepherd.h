#ifndef __SHEPHERD_H__
#define __SHEPHERD_H__

#include <QtCore>

enum class PendingCommand {
    none,
    moveRelative,
    moveAbsolute,
    home,
    send,
};

inline int operator+( PendingCommand const value ) { return static_cast<int>( value ); }

char const* ToString( PendingCommand const value );

class ProcessRunner;

class Shepherd: public QObject {

    Q_OBJECT

public:

    Shepherd( QObject* parent );
    virtual ~Shepherd( ) override;

    void start( );

    void doMoveRelative( float const relativeDistance, float const speed );
    void doMoveAbsolute( float const absolutePosition, float const speed );
    void doHome( );
    void doSend( QString cmd );
    void doSend( QStringList cmds );
    void doTerminate( );

protected:

private:

    QString        _buffer;
    QProcess*      _process               { };
    ProcessRunner* _processRunner         { };
    PendingCommand _pendingCommand        { PendingCommand::none };
    int            _okCount               { };
    int            _expectedOkCount       { };
    double         _zPosition             { 0.0 };
    bool           _isTerminationExpected { };

    QString        _stdoutBuffer;
    QString        _stderrBuffer;

    QMap<PendingCommand, std::function<void( bool const )>> _actionCompleteMap {
        { PendingCommand::moveRelative, [ this ] ( bool const success ) { emit action_moveRelativeComplete( success );                    } },
        { PendingCommand::moveAbsolute, [ this ] ( bool const success ) { emit action_moveAbsoluteComplete( success );                    } },
        { PendingCommand::home,         [ this ] ( bool const success ) { emit action_homeComplete( success );                            } },
        { PendingCommand::send,         [ this ] ( bool const success ) { emit action_sendComplete( success );                            } },
        { PendingCommand::none,         [ this ] ( bool const )         { debug( "+ Shepherd::handleFromPrinter: no pending command\n" ); } },
    };

    bool        getReady( char const* functionName, PendingCommand const pendingCommand, int const expectedOkCount = 0 );
    QStringList splitLine( QString const& line );
    void        handleFromPrinter( QString const& input );
    void        handleCommandFail( QStringList const& input );
#if defined _DEBUG
    void        handleCommandFailAlternate( QStringList const& input );
#endif // defined _DEBUG
    void        handleInput( QString const& input );

    void        doSendOne( QString& cmd );

    void        launchShepherd( );

signals:

    void shepherd_started( );
    void shepherd_startFailed( );
    void shepherd_terminated( bool const expected, bool const cleanExit );

    void printer_online( );
    void printer_offline( );
    void printer_output( QString const& output );
    void printer_positionReport( double const px, int const cx );
    void printer_temperatureReport( double const bedCurrentTemperature, double const bedTargetTemperature, int const bedPwm );
    void printer_firmwareVersionReport( QString const& version );

    void action_moveRelativeComplete( bool const successful );
    void action_moveAbsoluteComplete( bool const successful );
    void action_homeComplete( bool const successful );
    void action_sendComplete( bool const successful );

public slots:

protected slots:

private slots:

    void process_errorOccurred( QProcess::ProcessError error );
    void process_started( );
    void process_readyReadStandardError( );
    void process_readyReadStandardOutput( );
    void process_finished( int exitCode, QProcess::ExitStatus exitStatus );

    void processRunner_succeeded( );
    void processRunner_failed( int const exitCode, QProcess::ProcessError const processError );
    void processRunner_stdout( QString const& data );
    void processRunner_stderr( QString const& data );

};

#endif // __SHEPHERD_H__
