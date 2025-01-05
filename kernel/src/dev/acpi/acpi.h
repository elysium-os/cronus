#pragma once

#include <stdint.h>

typedef struct [[gnu::packed]] {
    uint8_t signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    uint8_t oem_id[6];
    uint8_t oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_sdt_header_t;

typedef struct [[gnu::packed]] {
    uint8_t address_space_id;
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;
    uint64_t address;
} acpi_generic_address_structure_t;

typedef struct [[gnu::packed]] {
    acpi_sdt_header_t common_header;
    uint32_t firmware_control;
    uint32_t dsdt;
    uint8_t rsv0;
    uint8_t preferred_power_management_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command_port;
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t c_state_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_architecture_flags;
    uint8_t rsv1;
    uint32_t flags;
    acpi_generic_address_structure_t reset_register;
    uint8_t reset_value;
    uint8_t rsv2[3];
    uint64_t x_firmware_control;
    uint64_t x_dsdt;
    acpi_generic_address_structure_t x_pm1a_event_block;
    acpi_generic_address_structure_t x_pm1b_event_block;
    acpi_generic_address_structure_t x_pm1a_control_block;
    acpi_generic_address_structure_t x_pm1b_control_block;
    acpi_generic_address_structure_t x_pm2_control_block;
    acpi_generic_address_structure_t x_pm_timer_block;
    acpi_generic_address_structure_t x_gpe0_block;
    acpi_generic_address_structure_t x_gpe1_block;
} acpi_fadt_t;

/**
 * @brief Initialize ACPI.
 * @param rsdp root system description pointer
 */
void acpi_init(uintptr_t rsdp);

/**
 * @brief Find ACPI table.
 */
acpi_sdt_header_t *acpi_find_table(uint8_t *signature);

/**
 * @brief Get ACPI revision.
 */
uint8_t acpi_revision();

/**
 * @brief Get ACPI rsdp.
 * @returns physical rsdp address OR zero
 */
uintptr_t acpi_rsdp();