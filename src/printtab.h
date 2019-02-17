#ifndef __PRINTTAB_H__
#define __PRINTTAB_H__

enum class BuildPlatformState {
    Extended,
    Retracting,
    Retracted,
    Extending,
};

class PrintJob;

class PrintTab: public QWidget {

    Q_OBJECT

public:

    PrintTab( QWidget* parent = nullptr );
    virtual ~PrintTab( ) override;

    PrintJob* printJob( ) const {
        return _printJob;
    }

    bool isPrintButtonEnabled( ) const {
        return printButton->isEnabled( );
    }

protected:

private:

    PrintJob*          _printJob                       { };
    BuildPlatformState _buildPlatformState             { BuildPlatformState::Extended };

    QLabel*            exposureTimeLabel               { new QLabel      };
    QLabel*            exposureTimeValue               { new QLabel      };
    QHBoxLayout*       exposureTimeValueLayout         { new QHBoxLayout };
    QWidget*           exposureTimeValueContainer      { new QWidget     };
    QDial*             exposureTimeDial                { new QDial       };
    QLabel*            exposureTimeDialLeftLabel       { new QLabel      };
    QLabel*            exposureTimeDialRightLabel      { new QLabel      };
    QHBoxLayout*       exposureTimeDialLabelsLayout    { new QHBoxLayout };
    QWidget*           exposureTimeDialLabelsContainer { new QWidget     };
    QLabel*            exposureTimeScaleFactorLabel    { new QLabel      };
    QComboBox*         exposureTimeScaleFactorComboBox { new QComboBox   };
    QLabel*            powerLevelLabel                 { new QLabel      };
    QLabel*            powerLevelValue                 { new QLabel      };
    QHBoxLayout*       powerLevelValueLayout           { new QHBoxLayout };
    QWidget*           powerLevelValueContainer        { new QWidget     };
    QDial*             powerLevelDial                  { new QDial       };
    QLabel*            powerLevelDialLeftLabel         { new QLabel      };
    QLabel*            powerLevelDialRightLabel        { new QLabel      };
    QHBoxLayout*       powerLevelDialLabelsLayout      { new QHBoxLayout };
    QWidget*           powerLevelDialLabelsContainer   { new QWidget     };
    QVBoxLayout*       optionsLayout                   { new QVBoxLayout };
    QWidget*           optionsContainer                { new QWidget     };
    QPushButton*       printButton                     { new QPushButton };

    QPushButton*       _adjustBedHeightButton          { new QPushButton };
    QPushButton*       _retractOrExtendButton          { new QPushButton };
    QPushButton*       _moveUpButton                   { new QPushButton };
    QPushButton*       _moveDownButton                 { new QPushButton };
    QHBoxLayout*       _adjustmentsHBox                { new QHBoxLayout };
    QVBoxLayout*       _adjustmentsVBox                { new QVBoxLayout };
    QGroupBox*         _adjustmentsGroup               { new QGroupBox   };

    QGridLayout*       _layout                         { new QGridLayout };

signals:

    void printButtonClicked( );
    void adjustBedHeight( double const newHeight );
    void retractBuildPlatform( );
    void extendBuildPlatform( );
    void moveBuildPlatformUp( );
    void moveBuildPlatformDown( );

public slots:

    void setPrintJob( PrintJob* printJob );
    void setPrintButtonEnabled( bool const value );

    void setAdjustmentButtonsEnabled( bool const value );

    void adjustBedHeightComplete( bool const success );
    void retractBuildPlatformComplete( bool const success );
    void extendBuildPlatformComplete( bool const success );
    void moveBuildPlatformUpComplete( bool const success );
    void moveBuildPlatformDownComplete( bool const success );

protected slots:

private slots:

    void exposureTimeDial_valueChanged( int value );
    void exposureTimeScaleFactorComboBox_currentIndexChanged( int index );
    void powerLevelDial_valueChanged( int value );
    void printButton_clicked( bool checked );
    void _adjustBedHeightButton_clicked( bool checked );
    void _retractOrExtendButton_clicked( bool checked );
    void _moveUpButton_clicked( bool checked );
    void _moveDownButton_clicked( bool checked );

};

#endif // __PRINTTAB_H__