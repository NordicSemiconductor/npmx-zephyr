.. _buck_retention_sample:

BUCK - Retention voltage setting
################################

.. contents::
   :local:
   :depth: 2

This BUCK sample variant demonstrates how to set retention voltage with the use of npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample uses the npmx BUCK driver to set the retention voltage for the buck converter with GPIO.
GPIO1 (:c:enumerator:`NPMX_GPIO_INSTANCE_1`) is set to the input mode and is configured to be used as a retention input selector for BUCK1 (:c:enumerator:`NPMX_BUCK_INSTANCE_1`).
The normal mode voltage is set to 1.7 V (:c:enumerator:`NPMX_BUCK_VOLTAGE_1V7`) and the retention voltage to 3.3 V (:c:enumerator:`NPMX_BUCK_VOLTAGE_3V3`).
When you change the input state of the selected GPIO, the voltage in the BUCK1 output will change.

Voltage can be changed only for BUCK1.
BUCK2 voltage is set by default to 3.0 V.

Wiring
******

1. Connect the TWI interface between the chosen DK and the nPM1300 EK as in the following table:

   .. list-table:: nPM1300 EK connections.
     :widths: 25 25 25
     :header-rows: 1

     * - nPM1300 EK pins
       - nRF5340 DK pins
       - nRF52840 DK pins
     * - GPIO0
       - P1.10
       - P1.10
     * - SDA
       - P1.02
       - P0.26
     * - SCL
       - P1.03
       - P0.27
     * - VOUT2 & GND
       - External supply (P21)
       - External supply (P21)

#. Make the following connections on the chosen DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

#. Make the following connections on the nPM1300 EK:

   .. include:: /includes/npm1300_ek_connections.txt

Configuration
*************

|config|

Building and running
********************

.. |sample path| replace:: :file:`samples/buck_retention`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

1. Connect a multimeter to VOUT1 that is the output voltage pin of BUCK1.
#. |connect_kit|
#. |connect_terminal|
#. Connect EK **GPIO1** pin to GND.
#. Check output voltage on EK **VOUT1** pin - it should be 1.7 V.
#. Connect EK **GPIO1** pin to VDD.
#. Check output voltage on EK **VOUT1** pin - it should be 3.3 V.
#. Verify output voltage on BUCK1.

Dependencies
************

This sample uses drivers from npmx.
