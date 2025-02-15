// Code common to all devices that communicate with a PMU via i2c
//
#include <driver/i2c.h>

static SemaphoreHandle_t i2c_mutex = NULL;

#define ACK_CHECK_EN 0x1 /*!< I2C master will check ack from slave*/
#define ACK_CHECK_DIS 0x0 /*!< I2C master will not check ack from slave */
#define ACK_VAL 0x0 /*!< I2C ack value */
#define NACK_VAL 0x1 /*!< I2C nack value */

#define I2C_CHECK_RET(expr)                                                                                            \
    do {                                                                                                               \
        const esp_err_t res = (expr);                                                                                  \
        if (res != ESP_OK) {                                                                                           \
            JADE_LOGE("i2c call returned: %u (%d)", res, res);                                                         \
            return res;                                                                                                \
        }                                                                                                              \
    } while (false)

#define I2C_LOG_ANY_ERROR(expr)                                                                                        \
    do {                                                                                                               \
        const esp_err_t res = (expr);                                                                                  \
        if (res != ESP_OK) {                                                                                           \
            JADE_LOGE("i2c call returned: %u (%d)", res, res);                                                         \
        }                                                                                                              \
    } while (false)

esp_err_t _i2c_init_master(
    const i2c_port_t port_num, const int sda_io_num, const int scl_io_num, const uint32_t clk_speed)
{
    const i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_io_num,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl_io_num,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = clk_speed,
        .clk_flags = 0,
    };
    I2C_CHECK_RET(i2c_param_config(port_num, &conf));
    I2C_CHECK_RET(i2c_driver_install(port_num, conf.mode, 0, 0, 0));

    return ESP_OK;
}

esp_err_t _i2c_deinit(const i2c_port_t port_num)
{
    I2C_CHECK_RET(i2c_driver_delete(port_num));
    return ESP_OK;
}

static esp_err_t _power_i2c_init(void)
{
    I2C_CHECK_RET(_i2c_init_master(I2C_NUM_0, CONFIG_I2C_SDA, CONFIG_I2C_SCL, CONFIG_I2C_MASTER_CLK));

    // Create i2c mutex semaphore
    i2c_mutex = xSemaphoreCreateMutex();
    JADE_ASSERT(i2c_mutex);

    return ESP_OK;
}

// NOTE: i2c_mutex must be claimed before calling
static esp_err_t _power_master_write_slave(uint8_t address, uint8_t* data_wr, size_t size)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    I2C_LOG_ANY_ERROR(i2c_master_start(cmd));
    I2C_LOG_ANY_ERROR(i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true));
    I2C_LOG_ANY_ERROR(i2c_master_write(cmd, data_wr, size, true));
    I2C_LOG_ANY_ERROR(i2c_master_stop(cmd));

    const esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    I2C_LOG_ANY_ERROR(ret);

    i2c_cmd_link_delete(cmd);
    return ret;
}

// NOTE: i2c_mutex must be claimed before calling
static esp_err_t _power_master_read_slave(uint8_t address, uint8_t register_address, uint8_t* data_rd, size_t size)
{
    if (size == 0) {
        return ESP_OK;
    }

    I2C_CHECK_RET(_power_master_write_slave(address, &register_address, 1));

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    I2C_LOG_ANY_ERROR(i2c_master_start(cmd));
    I2C_LOG_ANY_ERROR(i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_READ, ACK_CHECK_EN));
    if (size > 1) {
        I2C_LOG_ANY_ERROR(i2c_master_read(cmd, data_rd, size - 1, ACK_VAL));
    }

    I2C_LOG_ANY_ERROR(i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL));
    I2C_LOG_ANY_ERROR(i2c_master_stop(cmd));

    const esp_err_t ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_PERIOD_MS);
    I2C_LOG_ANY_ERROR(ret);

    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t _power_write_command(uint8_t address, uint8_t reg, uint8_t val)
{
    uint8_t arr[] = { reg, val };
    I2C_CHECK_RET(_power_master_write_slave(address, arr, 2));
    vTaskDelay(20 / portTICK_PERIOD_MS);
    return ESP_OK;
}
