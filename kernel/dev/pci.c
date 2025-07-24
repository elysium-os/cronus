#include "pci.h"

#include "arch/mmio.h"
#include "common/assert.h"
#include "common/log.h"
#include "lib/container.h"
#include "lib/list.h"
#include "memory/heap.h"
#include "memory/mmio.h"
#include "sys/init.h"

#include <uacpi/acpi.h>
#include <uacpi/tables.h>

#ifdef __ARCH_X86_64
#include "arch/x86_64/cpu/port.h"

#define PORT_CONFIG_ADDRESS 0xCF8
#define PORT_CONFIG_DATA 0xCFC
#endif

#define VENDOR_INVALID 0xFFFF
#define CMD_REG_BUSMASTER (1 << 2)
#define HEADER_TYPE_MULTIFUNC (1 << 7)

#define PCIE_DEVICE(PCI_DEVICE) CONTAINER_OF((PCI_DEVICE), pcie_device_t, common)

typedef struct [[gnu::packed]] {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t program_interface;
    uint8_t sub_class;
    uint8_t class;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
} pci_device_header_t;

typedef struct [[gnu::packed]] {
    pci_device_header_t device_header;

    uint32_t bar0;
    uint32_t bar1;
    uint32_t bar2;
    uint32_t bar3;
    uint32_t bar4;
    uint32_t bar5;

    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base_address;
    uint8_t capabilities_pointer;
    uint8_t rsv0;
    uint16_t rsv1;
    uint32_t rsv2;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
} pci_header0_t;

typedef struct [[gnu::packed]] {
    pci_device_header_t device_header;

    uint32_t bar0;
    uint32_t bar1;

    uint8_t primary_bus;
    uint8_t secondary_bus;
    uint8_t subordinate_bus;
    uint8_t secondary_latency_timer;
    uint8_t io_base_lower;
    uint8_t io_limit_lower;
    uint16_t secondary_status;
    uint16_t memory_base_lower;
    uint16_t memory_limit_lower;
    uint16_t prefetchable_memory_base;
    uint16_t prefetchable_memory_limit;
    uint32_t prefetchable_memory_base_upper;
    uint32_t prefetchable_memory_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capability_pointer;
    uint8_t rsv0;
    uint16_t rsv1;
    uint32_t expansion_rom_base;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t bridge_control;
} pci_header1_t;

typedef struct {
    uint64_t base_address;
    uint16_t segment_group_number;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
} pcie_segment_t;

typedef struct {
    pci_device_t common;
    void *config_space;
} pcie_device_t;

static size_t g_segment_count;
static pcie_segment_t *g_segments;

spinlock_t g_pci_devices_lock = SPINLOCK_INIT;
list_t g_pci_devices = LIST_INIT;

static pci_device_t *(*g_create_device)(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func);
static void (*g_free_device)(pci_device_t *device);

static uint32_t (*g_read)(pci_device_t *device, uint8_t offset, uint8_t size);
static void (*g_write)(pci_device_t *device, uint8_t offset, uint8_t size, uint32_t value);

static uint32_t readd(pci_device_t *device, uint8_t offset) {
    return g_read(device, offset, 4);
}

static uint16_t readw(pci_device_t *device, uint8_t offset) {
    return (uint16_t) g_read(device, offset, 2);
}

static uint16_t readb(pci_device_t *device, uint8_t offset) {
    return (uint8_t) g_read(device, offset, 1);
}

static void writed(pci_device_t *device, uint8_t offset, uint32_t value) {
    return g_write(device, offset, 4, value);
}

static void writew(pci_device_t *device, uint8_t offset, uint16_t value) {
    return g_write(device, offset, 2, value);
}

static void writeb(pci_device_t *device, uint8_t offset, uint8_t value) {
    return g_write(device, offset, 1, value);
}

static pci_device_t *pcie_create_device(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func) {
    pcie_device_t *device = heap_alloc(sizeof(pcie_device_t));
    device->common.segment = segment;
    device->common.bus = bus;
    device->common.slot = slot;
    device->common.func = func;

    size_t offset = ((uint32_t) (bus - g_segments[segment].start_bus_number) << 20) | ((uint32_t) (slot & 0x1F) << 15) | ((uint32_t) (func & 0x7) << 12);

    device->config_space = mmio_map(g_segments[segment].base_address + offset, 4096);

    return &device->common;
}

static void pcie_free_device(pci_device_t *device) {
    pcie_device_t *pcie_device = PCIE_DEVICE(device);
    mmio_unmap(pcie_device->config_space, 4096);
    heap_free(pcie_device, sizeof(pcie_device_t));
}

static uint32_t pcie_read(pci_device_t *device, uint8_t offset, uint8_t size) {
    void *address = PCIE_DEVICE(device)->config_space + offset;
    switch(size) {
        case 4: return arch_mmio_read32(address);
        case 2: return arch_mmio_read16(address);
        case 1: return arch_mmio_read8(address);
    }
    ASSERT_UNREACHABLE_COMMENT("invalid pcie read size");
}

static void pcie_write(pci_device_t *device, uint8_t offset, uint8_t size, uint32_t value) {
    void *address = PCIE_DEVICE(device)->config_space + offset;
    switch(size) {
        case 4: arch_mmio_write32(address, value); return;
        case 2: arch_mmio_write16(address, value); return;
        case 1: arch_mmio_write8(address, value); return;
    }
    ASSERT_UNREACHABLE_COMMENT("invalid pcie write size");
}

#ifdef __ARCH_X86_64
static pci_device_t *pci_create_device(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func) {
    pci_device_t *device = heap_alloc(sizeof(pci_device_t));
    device->segment = segment;
    device->bus = bus;
    device->slot = slot;
    device->func = func;
    return device;
}

static void pci_free_device(pci_device_t *device) {
    heap_free(device, sizeof(pci_device_t));
}

static uint32_t pci_read(pci_device_t *device, uint8_t offset, uint8_t size) {
    uint32_t register_offset = (offset & 0xFC) | (1 << 31);
    register_offset |= (uint32_t) device->bus << 16;
    register_offset |= (uint32_t) (device->slot & 0x1F) << 11;
    register_offset |= (uint32_t) (device->func & 0x7) << 8;
    x86_64_port_outd(PORT_CONFIG_ADDRESS, register_offset);

    uint32_t value = x86_64_port_ind(PORT_CONFIG_DATA);
    switch(size) {
        case 4: return value;
        case 2: return (uint16_t) (value >> ((offset & 2) * 8));
        case 1: return (uint8_t) (value >> ((offset & 3) * 8));
    }
    ASSERT_UNREACHABLE_COMMENT("invalid pci read size");
}

static void pci_write(pci_device_t *device, uint8_t offset, uint8_t size, uint32_t value) {
    uint32_t original = pci_read(device, offset, size);

    switch(size) {
        case 4: original = value; break;
        case 2:
            original &= ~(0xFFFF >> ((offset & 2) * 8));
            original |= (uint16_t) value >> ((offset & 2) * 8);
            break;
        case 1:
            original &= ~(0xFF >> ((offset & 3) * 8));
            original |= (uint8_t) value >> ((offset & 3) * 8);
            break;
        default: ASSERT_UNREACHABLE_COMMENT("invalid pci write size");
    }
    x86_64_port_outd(PORT_CONFIG_ADDRESS, ((uint32_t) device->bus << 16) | ((uint32_t) (device->slot & 0x1F) << 11) | ((uint32_t) (device->func & 0x7) << 8) | (offset & 0xFC) | (1 << 31));
    x86_64_port_outd(PORT_CONFIG_DATA, original);
}
#endif

static void enumerate_bus(uint16_t segment, uint8_t bus);

static void enumerate_function(uint16_t segment, uint8_t bus, uint8_t slot, uint8_t func) {
    pci_device_t *device = g_create_device(segment, bus, slot, func);

    uint16_t vendor_id = readw(device, offsetof(pci_device_header_t, vendor_id));
    if(vendor_id == VENDOR_INVALID) {
        g_free_device(device);
        return;
    }

    spinlock_acquire_nodw(&g_pci_devices_lock);
    list_push(&g_pci_devices, &device->list_node);
    spinlock_release_nodw(&g_pci_devices_lock);

    uint8_t class = readb(device, offsetof(pci_device_header_t, class));
    uint8_t sub_class = readb(device, offsetof(pci_device_header_t, sub_class));
    uint8_t prog_if = readb(device, offsetof(pci_device_header_t, program_interface));

    log(LOG_LEVEL_INFO, "PCI", "Enumerated PCI Device { VendorID: %#x, Class: %#x, SubClass: %#x, ProgIf: %#x }", vendor_id, class, sub_class, prog_if);

    if(class == 0x6 && sub_class == 0x4) {
        enumerate_bus(segment, (uint8_t) (readb(device, offsetof(pci_header1_t, secondary_bus)) >> 8));
        return;
    }

    // TODO: register devices
    // dev_driver_t *driver;
    // DEV_FOREACH(driver) {
    //     if(driver->type != DEV_DRIVER_PCI) continue;
    //     if((driver->pci->match & PCI_DRIVER_MATCH_CLASS) && driver->pci->class != class) continue;
    //     if((driver->pci->match & PCI_DRIVER_MATCH_SUBCLASS) && driver->pci->subclass != sub_class) continue;
    //     if((driver->pci->match & PCI_DRIVER_MATCH_FUNCTION) && driver->pci->prog_if != prog_if) continue;
    //     driver->pci->initialize(device);
    // }
}

static void enumerate_bus(uint16_t segment, uint8_t bus) {
    for(uint8_t slot = 0; slot < 32; slot++) {
        pci_device_t *device = g_create_device(segment, bus, slot, 0);
        if(readw(device, offsetof(pci_device_header_t, vendor_id)) == VENDOR_INVALID) continue;
        for(uint8_t func = 0; func < ((readb(device, offsetof(pci_device_header_t, header_type)) & HEADER_TYPE_MULTIFUNC) ? 8 : 1); func++) {
            enumerate_function(segment, bus, slot, func);
        }
        g_free_device(device);
    }
}

static void enumerate_segment(uint16_t segment) {
    pci_device_t *root_controller = g_create_device(segment, 0, 0, 0);
    if(readb(root_controller, offsetof(pci_device_header_t, header_type)) & HEADER_TYPE_MULTIFUNC) {
        for(uint8_t func = 0; func < 8; func++) {
            pci_device_t controller = { .segment = segment, .func = func };
            if(readw(&controller, offsetof(pci_device_header_t, vendor_id)) == VENDOR_INVALID) break;
            enumerate_bus(segment, func);
        }
    } else {
        enumerate_bus(segment, 0);
    }
    g_free_device(root_controller);
}

uint8_t pci_config_read_byte(pci_device_t *device, uint8_t offset) {
    return readb(device, offset);
}

uint16_t pci_config_read_word(pci_device_t *device, uint8_t offset) {
    return readw(device, offset);
}

uint32_t pci_config_read_double(pci_device_t *device, uint8_t offset) {
    return readd(device, offset);
}

void pci_config_write_byte(pci_device_t *device, uint8_t offset, uint8_t data) {
    writeb(device, offset, data);
}

void pci_config_write_word(pci_device_t *device, uint8_t offset, uint16_t data) {
    writew(device, offset, data);
}

void pci_config_write_double(pci_device_t *device, uint8_t offset, uint32_t data) {
    writed(device, offset, data);
}

pci_bar_t *pci_config_read_bar(pci_device_t *device, uint8_t index) {
    if(index > 5) return nullptr;

    pci_bar_t *new_bar = heap_alloc(sizeof(pci_bar_t));
    uint8_t offset = sizeof(pci_device_header_t) + index * sizeof(uint32_t);

    uint32_t bar = pci_config_read_double(device, offset);
    pci_config_write_double(device, offset, ~0);
    uint32_t size = pci_config_read_double(device, offset);
    pci_config_write_double(device, offset, bar);

    if(bar & 1) {
        new_bar->iospace = true;
        new_bar->address = (bar & ~0b11);
        new_bar->size = ~(size & ~0b11) + 1;
    } else {
        new_bar->iospace = false;
        new_bar->address = (bar & ~0b1111);
        new_bar->size = ~(size & ~0b1111) + 1;

        int type = ((bar >> 1) & 0b11);
        switch(type) {
            case 0:  break;
            case 2:  new_bar->address |= (uint64_t) pci_config_read_double(device, offset + sizeof(uint32_t)) << 32; break;
            default: heap_free(new_bar, sizeof(pci_bar_t)); return nullptr;
        }
    }
    return new_bar;
}

static void pci_enumerate() {
    struct acpi_mcfg *mcfg = nullptr;

    uacpi_table table;
    uacpi_status ret = uacpi_table_find_by_signature(ACPI_MCFG_SIGNATURE, &table);
    if(uacpi_likely_success(ret)) mcfg = (struct acpi_mcfg *) table.hdr;

    if(mcfg != nullptr) {
        log(LOG_LEVEL_INFO, "PCI", "Enumerating PCI-e Devices");

        g_create_device = pcie_create_device;
        g_free_device = pcie_free_device;
        g_read = pcie_read;
        g_write = pcie_write;

        g_segment_count = (mcfg->hdr.length - offsetof(struct acpi_mcfg, entries)) / sizeof(struct acpi_mcfg_allocation);
        g_segments = heap_alloc(sizeof(pcie_segment_t) * g_segment_count);
        for(size_t i = 0; i < g_segment_count; i++) {
            struct acpi_mcfg_allocation *allocation = &mcfg->entries[i];
            g_segments[i].base_address = allocation->address;
            g_segments[i].segment_group_number = allocation->segment;
            g_segments[i].start_bus_number = allocation->start_bus;
            g_segments[i].end_bus_number = allocation->end_bus;
            enumerate_segment(i); // FIX: fix on real hw
        }
    } else {
#ifdef __ARCH_X86_64
        log(LOG_LEVEL_INFO, "PCI", "Enumerating PCI Devices");
        g_create_device = pci_create_device;
        g_free_device = pci_free_device;
        g_read = pci_read;
        g_write = pci_write;
        enumerate_segment(0); // FIX: fix on real hw
#else
        log(LOG_LEVEL_ERROR, "PCI", "Both legacy PCI and PCIe unavailable");
#endif
    }

    if(uacpi_likely_success(ret)) uacpi_table_unref(&table);
}

INIT_TARGET(pci_enumerate, INIT_STAGE_DEV, pci_enumerate);
