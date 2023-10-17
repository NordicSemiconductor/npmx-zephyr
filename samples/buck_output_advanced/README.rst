.. _buck_output_advanced_sample:

BUCK - Advanced output voltage setting
######################################

.. contents::
   :local:
   :depth: 2

This BUCK sample variant demonstrates how to set advanced output voltage with the use of npmx drivers.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

The BUCK1 starts from 1.0 V (:c:enumerator:`NPMX_BUCK_VOLTAGE_1V0`), and the output voltage increases gradually every 200 ms to 3.3 V (:c:enumerator:`NPMX_BUCK_VOLTAGE_MAX`).

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

.. |sample path| replace:: :file:`samples/buck_output_advanced`

.. include:: /includes/build_and_run.txt

Testing
=======

|test_sample|

1. Connect a multimeter to VOUT1 that is the output voltage pin of BUCK1.
#. |connect_kit|
#. |connect_terminal|
#. Check output voltage on EK **VOUT1** pin - it should start from 1.0 V and increase every 200 ms up to 3.3 V.
#. Verify output voltage on BUCK1.

Dependencies
************

This sample uses drivers from npmx.
