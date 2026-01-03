import esphome.codegen as cg
from esphome.components import i2c, sensor, binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_PROBLEM,
    ICON_EMPTY,
    STATE_CLASS_MEASUREMENT,
)

DEPENDENCIES = ["i2c", "binary_sensor", "sensor"]

coffee_maker_ns = cg.esphome_ns.namespace("coffee_maker")
CoffeeMaker = coffee_maker_ns.class_("CoffeeMaker", cg.PollingComponent, i2c.I2CDevice)

# Binary Sensor Configurations
CONF_ONE_CUP_READY = "one_cup_ready"
CONF_TWO_CUP_READY = "two_cup_ready"
CONF_HOT_WATER = "hot_water"
CONF_WATER_EMPTY = "water_empty"
CONF_GROUNDS_FULL = "grounds_full"
CONF_ERROR = "error"
CONF_DECALCIFICATION_NEEDED = "decalcification_needed"
CONF_GRIND_DISABLED = "grind_disabled"

# Sensor Configurations
CONF_COFFEE_QUANTITY = "coffee_quantity"
CONF_COFFEE_FLAVOR = "coffee_flavor"

# GPIO Pin Configurations
CONF_PIN_CLOCK = "pin_clock"
CONF_PIN_DATA = "pin_data"
CONF_PIN_STROBE = "pin_strobe"
CONF_PIN_ZIO = "pin_zio"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CoffeeMaker),
            # GPIO Pins (as integer pin numbers)
            cv.Required(CONF_PIN_CLOCK): cv.int_range(0, 40),
            cv.Required(CONF_PIN_DATA): cv.int_range(0, 40),
            cv.Required(CONF_PIN_STROBE): cv.int_range(0, 40),
            cv.Required(CONF_PIN_ZIO): cv.int_range(0, 40),
            # Binary Sensors
            cv.Optional(CONF_ONE_CUP_READY): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_TWO_CUP_READY): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_HOT_WATER): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_WATER_EMPTY): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_GROUNDS_FULL): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_ERROR): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_DECALCIFICATION_NEEDED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            cv.Optional(CONF_GRIND_DISABLED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon=ICON_EMPTY,
            ),
            # Sensors
            cv.Optional(CONF_COFFEE_QUANTITY): sensor.sensor_schema(
                unit_of_measurement="cups",
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_EMPTY,
                accuracy_decimals=1,
            ),
            cv.Optional(CONF_COFFEE_FLAVOR): sensor.sensor_schema(
                unit_of_measurement="",
                state_class=STATE_CLASS_MEASUREMENT,
                icon=ICON_EMPTY,
                accuracy_decimals=0,
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x18))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    # GPIO Pin Configuration - extract pin numbers from validated config
    if CONF_PIN_CLOCK in config:
        pin_clock = config[CONF_PIN_CLOCK]
        cg.add(var.set_pin_clock(pin_clock))

    if CONF_PIN_DATA in config:
        pin_data = config[CONF_PIN_DATA]
        cg.add(var.set_pin_data(pin_data))

    if CONF_PIN_STROBE in config:
        pin_strobe = config[CONF_PIN_STROBE]
        cg.add(var.set_pin_strobe(pin_strobe))

    if CONF_PIN_ZIO in config:
        pin_zio = config[CONF_PIN_ZIO]
        cg.add(var.set_pin_zio(pin_zio))

    # Binary Sensors
    if CONF_ONE_CUP_READY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ONE_CUP_READY])
        cg.add(var.set_one_cup_ready(sens))

    if CONF_TWO_CUP_READY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_TWO_CUP_READY])
        cg.add(var.set_two_cup_ready(sens))

    if CONF_HOT_WATER in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_HOT_WATER])
        cg.add(var.set_hot_water(sens))

    if CONF_WATER_EMPTY in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_WATER_EMPTY])
        cg.add(var.set_water_empty(sens))

    if CONF_GROUNDS_FULL in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_GROUNDS_FULL])
        cg.add(var.set_grounds_full(sens))

    if CONF_ERROR in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_ERROR])
        cg.add(var.set_error(sens))

    if CONF_DECALCIFICATION_NEEDED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_DECALCIFICATION_NEEDED])
        cg.add(var.set_decalcification_needed(sens))

    if CONF_GRIND_DISABLED in config:
        sens = await binary_sensor.new_binary_sensor(config[CONF_GRIND_DISABLED])
        cg.add(var.set_grind_disabled(sens))

    # Sensors
    if CONF_COFFEE_QUANTITY in config:
        sens = await sensor.new_sensor(config[CONF_COFFEE_QUANTITY])
        cg.add(var.set_coffee_quantity(sens))

    if CONF_COFFEE_FLAVOR in config:
        sens = await sensor.new_sensor(config[CONF_COFFEE_FLAVOR])
        cg.add(var.set_coffee_flavor(sens))
