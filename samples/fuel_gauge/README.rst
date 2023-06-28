.. _npmx_fuel_gauge_sample:

Fuel Gauge
##########

.. contents::
   :local:
   :depth: 2

The npmx Fuel Gauge sample demonstrates how to calculate the battery State of Charge with the use of the `nRF Fuel Gauge library`_ and `npmx drivers`_.

Requirements
************

The sample supports the following development kits:

.. table-from-sample-yaml::

The sample also requires an nPM1300 EK.

Overview
********

This sample allows to calculate the State of Charge, Time to Empty and Time to Full levels from a battery connected to the nPM1300 PMIC.

Wiring
******

#. Connect the TWI interface between the chosen DK and the nPM1300 EK as in the following table:

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

#. Make the following connections on the nPM1300 EK:

   * Connect a USB power supply to the **J3** connector.
   * Connect a suitable battery to the **J2** connector.
   * On the **P18** pin header, connect **VOUT2** and **VDDIO** pins with a jumper.
   * On the **P2** pin header, connect **VBAT** and **VBATIN** pins with a jumper.
   * On the **P17** pin header, connect all LEDs with jumpers.
   * On the **P13** pin header, connect **RSET1** and **GND** pins with a jumper.
   * On the **P14** pin header, connect **RSET2** and **VSET2** pins with a jumper.

#. Make the following connections on the DK:

   * Set the **SW9** nRF power source switch to **VDD**.
   * Set the **SW10** VEXT → nRF switch to **ON**.

Configuration
*************

|config|

Configuration options
=====================

Check and configure the following sample-specific Kconfig options:

.. _CONFIG_CURRENT_LIMIT:

CONFIG_CURRENT_LIMIT
  This option changes the VBUSIN current limit.

.. _CONFIG_CHARGING_CURRENT:

CONFIG_CHARGING_CURRENT
  This option changes the battery charging current.

.. _CONFIG_DISCHARGING_CURRENT:

CONFIG_DISCHARGING_CURRENT
  This option changes the battery discharge current limit.

.. _CONFIG_TERMINATION_VOLTAGE_NORMAL:

CONFIG_TERMINATION_VOLTAGE_NORMAL
  This option changes the battery termination voltage in normal temperature.

.. _CONFIG_TERMINATION_VOLTAGE_WARM:

CONFIG_TERMINATION_VOLTAGE_WARM
  This option changes the battery termination voltage in warm temperature.

.. _CONFIG_THERMISTOR_RESISTANCE:

CONFIG_THERMISTOR_RESISTANCE
  This option changes the thermistor nominal resistance.

.. _CONFIG_THERMISTOR_BETA:

CONFIG_THERMISTOR_BETA
  This option changes the thermistor beta value.

Building and running
********************

.. |sample path| replace:: :file:`samples/fuel_gauge`

.. include:: /includes/build_and_run.txt

Testing
=======

#. |connect_kit|
#. |connect_terminal|

If the initialization was successful, the terminal displays the following message with status information:

.. code-block:: console

   [00:00:00.000,000] <inf> fuel_gauge_main: PMIC device OK.
   [00:00:00.000,000] <inf> fuel_gauge_main: Fuel gauge OK.
   [00:00:00.000,000] <inf> fuel_gauge: V: 4.121, I: -0.127, T: 23.26, SoC: 87.63, TTE: nan, TTF: 2373

.. _table::
   :widths: auto

   ======  ===============  ====================================================
   Symbol  Description      Units
   ======  ===============  ====================================================
   V       Battery voltage  Volts
   I       Current          Amperes (negative for charge, positive for discharge)
   T       Temperature      Degrees Celsius
   SoC     State of Charge  Percent
   TTE     Time to Empty    Seconds (may be NaN)
   TTF     Time to Full     Seconds (may be NaN)
   ======  ===============  ====================================================

Dependencies
************

This sample uses `sdk-nrfxlib`_ library and `npmx library`_.
