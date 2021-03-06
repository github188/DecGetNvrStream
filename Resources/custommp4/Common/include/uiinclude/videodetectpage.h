#ifndef _VIDEODETECTPAGE_H_
#define _VIDEODETECTPAGE_H_


#include "ui.h"


#define IDC_STATIC_VDETECT_CH				IDD_DIALOG_VIDEODETECT+2
#define IDC_STATIC_VDETECT_TYPE			IDD_DIALOG_VIDEODETECT+3
#define IDC_STATIC_VDETECT_REC				IDD_DIALOG_VIDEODETECT+4

#define IDC_COMBO_VDETECT_CH				IDD_DIALOG_VIDEODETECT+5
#define IDC_COMBO_VDETECT_TYPE			IDD_DIALOG_VIDEODETECT+6
#define IDC_STATIC_VDETECT_ALARMOUT		IDD_DIALOG_VIDEODETECT+7
#define IDC_STATIC_VDETECT_BUZZER			IDD_DIALOG_VIDEODETECT+8
#define IDC_CHECK_VDETECT_BUZZER			IDD_DIALOG_VIDEODETECT+9

#define IDC_STATIC_VDETECT_AREA			IDD_DIALOG_VIDEODETECT+10
#define IDC_BUTTON_VDETECT_AREA			IDD_DIALOG_VIDEODETECT+11

#define IDC_STATIC_VDETECT_SENSITIVITY	IDD_DIALOG_VIDEODETECT+12
#define IDC_COMBO_VDETECT_SENSITIVITY		IDD_DIALOG_VIDEODETECT+13

#define IDC_BUTTON_VDETECT_COPY			IDD_DIALOG_VIDEODETECT+14
#define IDC_COMBO_VDETECT_COPY			IDD_DIALOG_VIDEODETECT+15

#define IDC_BUTTON_VDETECT_OK				IDD_DIALOG_VIDEODETECT+16
#define IDC_BUTTON_VDETECT_CANCEL		IDD_DIALOG_VIDEODETECT+17

#define IDC_STATIC_VDETECT_DELAY			IDD_DIALOG_VIDEODETECT+18
#define IDC_COMBO_VDETECT_DELAY			IDD_DIALOG_VIDEODETECT+19

#define IDC_CHECK_VDETECT_RECCH1				IDD_DIALOG_VIDEODETECT+21
#define  IDC_CHECK_VDETECT_RECCH2				IDD_DIALOG_VIDEODETECT+22
#define  IDC_CHECK_VDETECT_RECCH3				IDD_DIALOG_VIDEODETECT+23
#define  IDC_CHECK_VDETECT_RECCH4				IDD_DIALOG_VIDEODETECT+24
#define  IDC_CHECK_VDETECT_RECCH5				IDD_DIALOG_VIDEODETECT+25
#define  IDC_CHECK_VDETECT_RECCH6				IDD_DIALOG_VIDEODETECT+26
#define  IDC_CHECK_VDETECT_RECCH7				IDD_DIALOG_VIDEODETECT+27
#define  IDC_CHECK_VDETECT_RECCH8				IDD_DIALOG_VIDEODETECT+28
#define  IDC_CHECK_VDETECT_RECCH9				IDD_DIALOG_VIDEODETECT+29
#define  IDC_CHECK_VDETECT_RECCH10			IDD_DIALOG_VIDEODETECT+30
#define  IDC_CHECK_VDETECT_RECCH11			IDD_DIALOG_VIDEODETECT+31
#define  IDC_CHECK_VDETECT_RECCH12			IDD_DIALOG_VIDEODETECT+32
#define  IDC_CHECK_VDETECT_RECCH13			IDD_DIALOG_VIDEODETECT+33
#define  IDC_CHECK_VDETECT_RECCH14			IDD_DIALOG_VIDEODETECT+34
#define  IDC_CHECK_VDETECT_RECCH15			IDD_DIALOG_VIDEODETECT+35
#define  IDC_CHECK_VDETECT_RECCH16			IDD_DIALOG_VIDEODETECT+36


#define IDC_CHECK_VDETECT_ALARMOUT1			IDD_DIALOG_VIDEODETECT+38
#define IDC_CHECK_VDETECT_ALARMOUT2			IDD_DIALOG_VIDEODETECT+39
#define IDC_CHECK_VDETECT_ALARMOUT3			IDD_DIALOG_VIDEODETECT+40
#define IDC_CHECK_VDETECT_ALARMOUT4			IDD_DIALOG_VIDEODETECT+41
#define IDC_CHECK_VDETECT_ALARMOUT5			IDD_DIALOG_VIDEODETECT+42
#define IDC_CHECK_VDETECT_ALARMOUT6			IDD_DIALOG_VIDEODETECT+43
#define IDC_CHECK_VDETECT_ALARMOUT7			IDD_DIALOG_VIDEODETECT+44
#define IDC_CHECK_VDETECT_ALARMOUT8			IDD_DIALOG_VIDEODETECT+45
#define IDC_CHECK_VDETECT_ALARMOUT9			IDD_DIALOG_VIDEODETECT+46
#define IDC_CHECK_VDETECT_ALARMOUT10			IDD_DIALOG_VIDEODETECT+47
#define IDC_CHECK_VDETECT_ALARMOUT11			IDD_DIALOG_VIDEODETECT+48
#define IDC_CHECK_VDETECT_ALARMOUT12			IDD_DIALOG_VIDEODETECT+49
#define IDC_CHECK_VDETECT_ALARMOUT13			IDD_DIALOG_VIDEODETECT+50
#define IDC_CHECK_VDETECT_ALARMOUT14			IDD_DIALOG_VIDEODETECT+51
#define IDC_CHECK_VDETECT_ALARMOUT15			IDD_DIALOG_VIDEODETECT+52
#define IDC_CHECK_VDETECT_ALARMOUT16			IDD_DIALOG_VIDEODETECT+53

#define IDC_STATIC_VDETECT_MAILALARM			IDD_DIALOG_VIDEODETECT+54//zlb20090915
#define IDC_CHECK_VDETECT_MAILALARM			IDD_DIALOG_VIDEODETECT+55//zlb20090915

#ifdef VL_VB_DEAL
#define IDC_STATIC_VDETECT_VL_VB_DEAL			IDD_DIALOG_VIDEODETECT+59
#define IDC_CHECK_VDETECT_VL_VB_DEAL			IDD_DIALOG_VIDEODETECT+60
#endif

//pw 2010/6/30
#ifdef BOUNCING_PICTURE
#define IDC_CHECK_VDTECT_BUNC_PIC_STATIC		IDD_DIALOG_VIDEODETECT+61
#define IDC_CHECK_VDTECT_BUNC_PIC				IDD_DIALOG_VIDEODETECT+62
#endif

#ifdef FTP_PIC
#define IDC_STATIC_VDETECT_FTPALARM				IDD_DIALOG_VIDEODETECT+63
#define IDC_CHECK_VDETECT_FTPALARM				IDD_DIALOG_VIDEODETECT+64
#endif


BOOL CreateVideodetectPage();
BOOL ShowVideodetectPage();
#endif
