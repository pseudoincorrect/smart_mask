/**
 * @file
 * @brief sensor_sampling: Module that takes care of the sampling
 * of the sensors through ADC with a periodic interupts. it can
 * also be configured to generated mock values.
 */

/*************************
 * Includes
 ************************/

#include "sensor_sampling.h"
#include "app_error.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrfx_ppi.h"
#include "nrfx_rng.h"
#include "nrfx_saadc.h"
#include "nrfx_timer.h"
#include "sensor_handle.h"

#if (MOCK_ADC)
#include "nrf_drv_rng.h"
#endif

/*************************
 * Defines
 ************************/

#define SAMPLES_IN_BUFFER SENSORS_COUNT
#define INITIAL_SAMPLE_RATE_MS 200

/*************************
 * Static Variables
 ************************/

static const nrfx_timer_t saadc_timer_instance = NRFX_TIMER_INSTANCE(2);
static int mock_adc;

/*************************
 * Static Functions
 ************************/

/**
 * @brief Unsued but mandatory callback function for the ADC
 *
 * @param[in] p_event   pointer to a ADC event
 */
static void saadc_callback(nrfx_saadc_evt_t const * p_event)
{
    NRF_LOG_INFO("saadc_callback, p_event->type = %d", p_event->type);
}

/**ef
 * @brief configure an ADC channel (gain, reference, polarity, etc..) for
 *        a particular sensor
 *
 * @param[in] sensor    Selected sensor
 * @param[in] ctrl      Pointer to a sensor control handle
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
ret_code_t saadc_config_channel(sensor_t sensor)
{
    sensor_hardware_t * hardware = sensor_handle_get_hardware(sensor);
    sensor_ctrl_t * ctrl = sensor_handle_get_control(sensor);
    nrf_saadc_channel_config_t conf =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NULL);

    conf.pin_p = hardware->analog_input;
    conf.gain = ctrl->gain;
    //conf.reference = NRF_SAADC_REFERENCE_VDD4;
    conf.reference = NRF_SAADC_REFERENCE_INTERNAL;
    return nrfx_saadc_channel_init(hardware->adc_chanel, &conf);
}

/**
 * @brief initialize the ADC module and ADC channel for each sensor
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
static ret_code_t saadc_init(void)
{
    ret_code_t err;

    static nrfx_saadc_config_t default_config = NRFX_SAADC_DEFAULT_CONFIG;
    default_config.resolution = NRF_SAADC_RESOLUTION_12BIT;

    err = nrfx_saadc_init(&default_config, saadc_callback);
    APP_ERROR_CHECK(err);

    sensor_hardware_t * hardware;
    nrf_saadc_channel_config_t conf =
        NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NULL);

    for (sensor_t s_i = SENSOR_FIRST; s_i <= SENSOR_LAST; s_i++)
    {
        hardware = sensor_handle_get_hardware(s_i);
        err = saadc_config_channel(s_i);
        APP_ERROR_CHECK(err);
        nrf_gpio_cfg_output(hardware->pwr_pin);
    }

    return NRF_SUCCESS;
}

/**
 * @brief Sample the ADC channel for one sensor
 *
 * @param[in] sensor    Selected sensor
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
static ret_code_t sample_one_sensor(sensor_t sensor)
{
    ret_code_t err;
    nrf_saadc_value_t adc_val;
    sensor_hardware_t * hardware = sensor_handle_get_hardware(sensor);
    nrf_gpio_pin_set(hardware->pwr_pin);
    nrf_delay_ms(1);
    err = nrfx_saadc_sample_convert(hardware->adc_chanel, &adc_val);
    APP_ERROR_CHECK(err);
    err = sensor_handle_add_value(sensor, adc_val);
    APP_ERROR_CHECK(err);
    nrf_gpio_pin_clear(hardware->pwr_pin);
    //NRF_LOG_INFO("sensor %d adc_val %d", sensor + 1, adc_val);

    return err;
}

/**
 * @brief Sample the ADC channels for all sensors
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
static ret_code_t sample_all_sensors(void)
{
    ret_code_t err;
    sensor_ctrl_t * ctrl;

    ctrl = sensor_handle_get_control(SENSOR_1);
    if (ctrl->enable)
    {
        //err = sensor_handle_add_value(SENSOR_1, 0);
         err = sample_one_sensor(SENSOR_1);
        APP_ERROR_CHECK(err);
    }

    ctrl = sensor_handle_get_control(SENSOR_2);
    if (ctrl->enable)
    {
        //err = sample_one_sensor(SENSOR_2);
        err = sensor_handle_add_value(SENSOR_2, 0);
        APP_ERROR_CHECK(err);
    }

    ctrl = sensor_handle_get_control(SENSOR_3);
    if (ctrl->enable)
    {
        // err = nrfx_saadc_sample_convert(SENSOR_3_ADC_CHANNEL, &adc_val);
        err = sensor_handle_add_value(SENSOR_3, mock_adc++);
        APP_ERROR_CHECK(err);
    }

    ctrl = sensor_handle_get_control(SENSOR_4);
    if (ctrl->enable)
    {
        // err = nrfx_saadc_sample_convert(SENSOR_4_ADC_CHANNEL, &adc_val);
        err = sensor_handle_add_value(SENSOR_4, 0);
        APP_ERROR_CHECK(err);
    }

    return err;
}

/**
 * @brief Timer handler for time event (periodic) for the ADC
 *
 * @param[in] event_type Timer event (compare)
 * @param[in] p_context  Can be used to pass extra arguments to the handler
 */
static void saadc_timer_handler(nrf_timer_event_t event_type, void * p_context)
{
    switch (event_type)
    {
        case NRF_TIMER_EVENT_COMPARE0:
            sample_all_sensors();
            break;

        default:
            break;
    }
}

/**
 * @brief Initialize the timer for the periodic ADC sampling
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
static ret_code_t saadc_timer_init(void)
{
    ret_code_t err_code;
    nrfx_timer_config_t timer_conf = NRFX_TIMER_DEFAULT_CONFIG;
    timer_conf.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err_code = nrfx_timer_init(
        &saadc_timer_instance, &timer_conf, saadc_timer_handler);
    APP_ERROR_CHECK(err_code);

    uint32_t time_ticks =
        nrfx_timer_ms_to_ticks(&saadc_timer_instance, INITIAL_SAMPLE_RATE_MS);

    nrfx_timer_extended_compare(&saadc_timer_instance, NRF_TIMER_CC_CHANNEL0,
        time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);

    nrfx_timer_enable(&saadc_timer_instance);
}

/**
 * @brief Update the time for the ADC sampling, especially the frequency
 *
 * @param[in] sensor    Selected sensor
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
static ret_code_t update_saadc_timer(sensor_t sensor)
{
    ret_code_t err;

    nrfx_timer_uninit(&saadc_timer_instance);

    nrfx_timer_config_t timer_conf = NRFX_TIMER_DEFAULT_CONFIG;
    timer_conf.bit_width = NRF_TIMER_BIT_WIDTH_32;
    err = nrfx_timer_init(
        &saadc_timer_instance, &timer_conf, saadc_timer_handler);
    APP_ERROR_CHECK(err);

    sensor_ctrl_t * ctrl = sensor_handle_get_control(sensor);
    uint32_t time_ticks =
        nrfx_timer_ms_to_ticks(&saadc_timer_instance, ctrl->sample_period_ms);

    nrfx_timer_extended_compare(&saadc_timer_instance, NRF_TIMER_CC_CHANNEL0,
        time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);

    nrfx_timer_enable(&saadc_timer_instance);

    return NRF_SUCCESS;
}

#if (MOCK_ADC)
/**
 * @brief Generate a vector/array of random number
 *
 * @param[in] p_buff    Pointer to a buffer where random number will be copied
 * @param[in] size      Amount of random number to be generated
 *
 * @retval Amount of random numbers that have been generated
 */
static uint8_t random_vector_generate(uint8_t * p_buff, uint8_t size)
{
    uint32_t err_code;
    uint8_t available;
    nrf_drv_rng_bytes_available(&available);
    uint8_t length = MIN(size, available);
    err_code = nrf_drv_rng_rand(p_buff, length);
    APP_ERROR_CHECK(err_code);
    return length;
}

/**
 * @brief  initialize the random number module
 */
static void random_vector_generate_init(void)
{
    uint32_t err_code;
    err_code = nrf_drv_rng_init(NULL);
    APP_ERROR_CHECK(err_code);
}

/**
 * @brief generate random values for each sensors
 *
 * @param[in] sensors   Array of sensors
 */
static void mock_sensor_values(sensors_t * sensors)
{
    uint8_t randoms[SENSORS_COUNT];
    random_vector_generate(randoms, SENSORS_COUNT);
    for (int i = 0; i < SENSORS_COUNT; i++)
        sensors->values[i] = randoms[i];
}
#endif

/*************************
 * Public Functions
 ************************/

/**
 * @brief Initialize the sensor sampling module: ADC, timer, sensor handles
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
ret_code_t sensor_sampling_init(void)
{
#if (MOCK_ADC)
    random_vector_generate_init();
#else

    mock_adc = 0;
    sensor_handles_init();
    // init_sensor_buffer();
    saadc_init();
    saadc_timer_init();
#endif
    return NRF_SUCCESS;
}

/**
 * @brief update the ADC configuration/control for particuliar sensor
 *
 * @param[in] sensor    Selected sensor
 * @param[in] ctrl      Pointer to a sensor control handle with new values
 *
 * @retval NRF_SUCCESS on success, otherwise an error code is returned
 */
ret_code_t sensor_sampling_update_sensor_control(
    sensor_t sensor, sensor_ctrl_t * new_sensor_ctrl)
{
    ret_code_t err;
    if (!is_sensor_ctrl_valid(new_sensor_ctrl))
        return NRF_ERROR_INVALID_DATA;

    sensor_hardware_t * hardware = sensor_handle_get_hardware(sensor);
    err = nrfx_saadc_channel_uninit(hardware->adc_chanel);
    APP_ERROR_CHECK(err);

    err = sensor_handle_set_control(sensor, new_sensor_ctrl);
    APP_ERROR_CHECK(err);

    err = saadc_config_channel(sensor);
    APP_ERROR_CHECK(err);

    err = update_saadc_timer(sensor);
    APP_ERROR_CHECK(err);

    return NRF_SUCCESS;
}