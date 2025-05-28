#include <inttypes.h>

#include <zephyr/device.h>
#include <zephyr/drivers/can.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_device.h>

BUILD_ASSERT(DT_NODE_HAS_COMPAT(DT_CHOSEN(zephyr_console), zephyr_cdc_acm_uart),
	     "Console device is not ACM CDC UART device");

uint8_t battery_heating_active;
uint8_t heating_request;
uint8_t cooling_request;
uint8_t power_battery_heating_watt;
uint8_t power_battery_heating_req_watt;

uint8_t temperature_status_charge;

double max_charge_power_kw;
double max_charge_current_amp;
double battery_min_temp;
double battery_max_temp;

bool session_error = false;

const struct device *can_dev;

static struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios,
                                                     {0});
static struct gpio_dt_spec rx_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led1), gpios,
                                                     {0});
static struct gpio_dt_spec tx_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led2), gpios,
                                                     {0});

void log_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    printk("Got data (%08X) (%08x):", frame->id, frame->flags);
    for (int i=0; i<frame->dlc; ++i) {
        printk(" %02X", frame->data[i]);
    }
    printk("\n");
}

void diag_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    if (frame->data[0] == 0x03 &&
        frame->data[1] == 0x7F &&
        frame->data[2] == 0x2F &&
        frame->data[3] == 0x7F) {
        session_error = true;
    }
}

#define TEMPERATURE_STATUS_UNDER_OPTIMAL 1

void heating_status_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    battery_heating_active = (frame->data[4] & 0x40) >> 6;
    heating_request = (frame->data[5] & 0xE0) >> 5;
    cooling_request = (frame->data[5] & 0x1C) >> 2;
    power_battery_heating_watt = frame->data[6];
    power_battery_heating_req_watt = frame->data[7];
}

void charging_optimization_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    //0 init, 1 temp under optimal, 2 temp optimal, 3 temp over optimal, 7 fault
    temperature_status_charge = (((frame->data[2] & 0x03) << 1) | frame->data[1] >> 7);
}

void dynamic_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    max_charge_power_kw = ((frame->data[7] << 5) | (frame->data[6] >> 3)) * .1;
    max_charge_current_amp = (((frame->data[4] & 0x3F) << 7) | (frame->data[3] >> 1)) * 0.2;
}

void temp_cb(const struct device *dev, struct can_frame *frame, void *user_data)
{
    battery_max_temp = frame->data[3] * 0.5 - 40;
    battery_min_temp = frame->data[4] * 0.5 - 40;
}

void tx_irq_callback(const struct device *dev, int error, void *arg)
{
	char *sender = (char *)arg;

	ARG_UNUSED(dev);

	if (error != 0) {
		printk("Callback! error-code: %d\nSender: %s\n", error, sender);
	}
}

void diag_session_handler(struct k_work *work) {
    struct can_frame frame = {0};
    frame.id = 0x17fc007b;
    frame.flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS;
    frame.dlc = 8;
    frame.data[0] = 0x02;
    frame.data[1] = 0x10;
    frame.data[2] = 0x03;
    frame.data[3] = 0x00;
    frame.data[4] = 0x00;
    frame.data[5] = 0x00;
    frame.data[6] = 0x00;
    frame.data[7] = 0x00;
    can_send(can_dev, &frame, K_MSEC(500), tx_irq_callback, NULL);
}

void heat_request_handler(struct k_work *work) {
    struct can_frame frame = {0};
    frame.id = 0x17fc007b;
    frame.flags = CAN_FRAME_IDE | CAN_FRAME_FDF | CAN_FRAME_BRS;
    frame.dlc = 8;
    frame.data[0] = 0x07;
    frame.data[1] = 0x2F;
    frame.data[2] = 0x80;
    frame.data[3] = 0x37;
    frame.data[4] = 0x03;
    frame.data[5] = 0x00;
    frame.data[6] = 0x05;
    frame.data[7] = 0x32;
    can_send(can_dev, &frame, K_MSEC(500), tx_irq_callback, NULL);
}

K_WORK_DEFINE(diag_session_work, diag_session_handler);
K_WORK_DEFINE(heat_request_work, heat_request_handler);

void state_machine_cb(struct k_timer *timer) {
    if (temperature_status_charge == TEMPERATURE_STATUS_UNDER_OPTIMAL) {
        if (session_error) {
            k_work_submit(&diag_session_work);
            printk("Got session error, switching");
            session_error = false;
        } else {
            printk("Sending heat request");
            k_work_submit(&heat_request_work);
        }
        gpio_pin_set_dt(&tx_led, 1);
    } else {
        gpio_pin_set_dt(&tx_led, 0);
    }
}

void debug_cb(struct k_timer *timer) {
    printk("Heating status: %d %d, %d%% (%d%%)\n", battery_heating_active, heating_request, power_battery_heating_watt, power_battery_heating_req_watt);
    printk("Thermal status: %d\n", temperature_status_charge);
    printk("Predicted power: %.1fkW (%.1fA)\n", max_charge_power_kw, max_charge_current_amp);
    printk("Battery temp min/max: %.1fC / %.1fC\n", battery_min_temp, battery_max_temp);
    printk("\n");
}

K_TIMER_DEFINE(state_machine_timer, state_machine_cb, NULL);
K_TIMER_DEFINE(debug_timer, debug_cb, NULL);

int main(void)
{
	if (usb_enable(NULL)) {
		return 0;
	}

    gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
    gpio_pin_configure_dt(&rx_led, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&tx_led, GPIO_OUTPUT_INACTIVE);

    can_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_canbus));

    can_mode_t mode = CAN_MODE_FD;
    can_set_mode(can_dev, mode);

    can_start(can_dev);

    const struct can_filter diag_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0x17fe007b,
        .mask = CAN_EXT_ID_MASK,
    };

    const struct can_filter heating_status_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0x12DD54D2,
        .mask = CAN_EXT_ID_MASK,
    };
    const struct can_filter charging_optimization_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0x1A5555B2,
        .mask = CAN_EXT_ID_MASK,
    };
    const struct can_filter dynamic_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0x12DD54D0,
        .mask = CAN_EXT_ID_MASK,
    };
    const struct can_filter temp_filter = {
        .flags = CAN_FILTER_IDE,
        .id = 0x16A954A6,
        .mask = CAN_EXT_ID_MASK,
    };

    can_add_rx_filter(can_dev, diag_cb, NULL, &diag_filter);
    can_add_rx_filter(can_dev, heating_status_cb, NULL, &heating_status_filter);
    can_add_rx_filter(can_dev, charging_optimization_cb, NULL, &charging_optimization_filter);
    can_add_rx_filter(can_dev, dynamic_cb, NULL, &dynamic_filter);
    can_add_rx_filter(can_dev, temp_cb, NULL, &temp_filter);

    k_timer_start(&debug_timer, K_MSEC(1000), K_MSEC(1000));
    k_timer_start(&state_machine_timer, K_MSEC(250), K_MSEC(500));

    while (1)
    {
        gpio_pin_set_dt(&led, 0);
        k_msleep(SLEEP_TIME_OFF_MS);
        gpio_pin_set_dt(&led, 1);
        k_msleep(SLEEP_TIME_ON_MS);
    }

    return 0;
}
