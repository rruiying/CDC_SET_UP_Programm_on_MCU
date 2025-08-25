/*******************************************************************************
* File Name: OneWire_Pin.h  
* Version 2.20
*
* Description:
*  This file contains the Alias definitions for Per-Pin APIs in cypins.h. 
*  Information on using these APIs can be found in the System Reference Guide.
*
* Note:
*
********************************************************************************
* Copyright 2008-2015, Cypress Semiconductor Corporation.  All rights reserved.
* You may use this file only in accordance with the license, terms, conditions, 
* disclaimers, and limitations in the end user license agreement accompanying 
* the software package with which this file was provided.
*******************************************************************************/

#if !defined(CY_PINS_OneWire_Pin_ALIASES_H) /* Pins OneWire_Pin_ALIASES_H */
#define CY_PINS_OneWire_Pin_ALIASES_H

#include "cytypes.h"
#include "cyfitter.h"
#include "cypins.h"


/***************************************
*              Constants        
***************************************/
#define OneWire_Pin_0			(OneWire_Pin__0__PC)
#define OneWire_Pin_0_PS		(OneWire_Pin__0__PS)
#define OneWire_Pin_0_PC		(OneWire_Pin__0__PC)
#define OneWire_Pin_0_DR		(OneWire_Pin__0__DR)
#define OneWire_Pin_0_SHIFT	(OneWire_Pin__0__SHIFT)
#define OneWire_Pin_0_INTR	((uint16)((uint16)0x0003u << (OneWire_Pin__0__SHIFT*2u)))

#define OneWire_Pin_INTR_ALL	 ((uint16)(OneWire_Pin_0_INTR))


#endif /* End Pins OneWire_Pin_ALIASES_H */


/* [] END OF FILE */
