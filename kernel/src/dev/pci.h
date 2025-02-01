#pragma once

#include "common/lock/spinlock.h"
#include "dev/acpi/acpi.h"
#include "lib/list.h"

#include <stdint.h>

#define PCI_DRIVER_MATCH_CLASS (1 << 0)
#define PCI_DRIVER_MATCH_SUBCLASS (1 << 1)
#define PCI_DRIVER_MATCH_FUNCTION (1 << 2)

typedef struct {
    uint64_t address;
    uint32_t size;
    bool iospace;
} pci_bar_t;

typedef struct pci_device {
    uint16_t segment;
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    list_t list;
} pci_device_t;

typedef struct {
    void (*initialize)(pci_device_t *device);
    uint8_t match;
    uint16_t class;
    uint16_t subclass;
    uint16_t prog_if;
} pci_driver_t;

extern spinlock_t g_pci_devices_lock;
extern list_t g_pci_devices;

/**
 * @brief Enumerate PCI devices.
 * @param mcfg MCFG table or NULL
 */
void pci_enumerate(acpi_sdt_header_t *mcfg);

/**
 * @brief Read byte from device config.
 */
uint8_t pci_config_read_byte(pci_device_t *device, uint8_t offset);

/**
 * @brief Read word from device config.
 */
uint16_t pci_config_read_word(pci_device_t *device, uint8_t offset);

/**
 * @brief Read doubleword from device config.
 */
uint32_t pci_config_read_double(pci_device_t *device, uint8_t offset);

/**
 * @brief Write byte to device config.
 */
void pci_config_write_byte(pci_device_t *device, uint8_t offset, uint8_t data);

/**
 * @brief Write word to device config.
 */
void pci_config_write_word(pci_device_t *device, uint8_t offset, uint16_t data);

/**
 * @brief Write doubleword to device config.
 */
void pci_config_write_double(pci_device_t *device, uint8_t offset, uint32_t data);

/**
 * @brief Read BAR register from device config.
 * @warning Heap allocation on success.
 * @param index BAR index (0-5)
 * @return PCI bar on success, 0 on failure
 */
pci_bar_t *pci_config_read_bar(pci_device_t *device, uint8_t index);
