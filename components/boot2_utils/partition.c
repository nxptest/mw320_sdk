/** @file partition.c
 *
 *  @brief This file provides  Flash Partition Manager
 *
 *  Copyright 2008-2020 NXP
 *
 *  NXP CONFIDENTIAL
 *  The source code contained or described herein and all documents related to
 *  the source code ("Materials") are owned by NXP, its
 *  suppliers and/or its licensors. Title to the Materials remains with NXP,
 *  its suppliers and/or its licensors. The Materials contain
 *  trade secrets and proprietary and confidential information of NXP, its
 *  suppliers and/or its licensors. The Materials are protected by worldwide copyright
 *  and trade secret laws and treaty provisions. No part of the Materials may be
 *  used, copied, reproduced, modified, published, uploaded, posted,
 *  transmitted, distributed, or disclosed in any way without NXP's prior
 *  express written permission.
 *
 *  No license under any patent, copyright, trade secret or other intellectual
 *  property right is granted to or conferred upon you by disclosure or delivery
 *  of the Materials, either expressly, by implication, inducement, estoppel or
 *  otherwise. Any license under such intellectual property rights must be
 *  express and approved by NXP in writing.
 *
 */

#include "crc32.h"
#include "boot_flags.h"
#include "partition.h"
#include "flash_layout.h"
#include "mflash_drv.h"
#include "fsl_debug_console.h"

struct partition_table g_flash_table;
struct partition_entry g_flash_parts[MAX_FL_COMP];
static uint32_t g_active_part_addr;

static inline uint32_t part_get_table_addr_from_id(bool part_id)
{
    return (part_id == 0) ? FL_PART1_START : FL_PART2_START;
}

static inline uint32_t part_get_passive_table_addr()
{
    return (g_active_part_addr == FL_PART1_START) ? FL_PART2_START : FL_PART1_START;
}

static int part_read_layout()
{
    int read_size, ret;
    uint32_t crc;
    uint32_t part_addr;

    part_addr = g_active_part_addr;
    crc32_init();

    ret = mflash_drv_read(part_addr, (uint32_t *)&g_flash_table, sizeof(struct partition_table));
    if (kStatus_Success != ret)
    {
        assert(false);
    }

    crc = crc32(&g_flash_table, sizeof(g_flash_table), 0);
    if (crc != 0)
    {
        PRINTF("CRC mismatch");
        return -WM_E_CRC;
    }

    if (g_flash_table.magic != PARTITION_TABLE_MAGIC)
    {
        PRINTF("magic number mismatch");
        return -WM_FAIL;
    }
    if (g_flash_table.version != PARTITION_TABLE_VERSION)
    {
        PRINTF("Partition table version mismatch");
        return -WM_FAIL;
    }
    part_addr += sizeof(struct partition_table);

    if (g_flash_table.partition_entries_no > MAX_FL_COMP)
    {
        PRINTF(
            "Only %d partition entries are supported."
            " Current = %d. Truncating ...",
            MAX_FL_COMP, g_flash_table.partition_entries_no);
        g_flash_table.partition_entries_no = MAX_FL_COMP;
    }

    read_size = g_flash_table.partition_entries_no * sizeof(struct partition_entry);
    ret = mflash_drv_read(part_addr, (uint32_t *)g_flash_parts, read_size);
    if (kStatus_Success != ret)
    {
        assert(false);
    }

    return WM_SUCCESS;
}

int part_init(void)
{
    static bool part_init_done;
    bool act_part_id;

    if (part_init_done == true)
        return WM_SUCCESS;

    if (((g_boot_flags & PARTITION_TABLE1_STATE_MASK) != 0U) || ((g_boot_flags & PARTITION_TABLE2_STATE_MASK) != 0U))
    {
        /* Not valid boot flags, use part 0. */
        act_part_id = 0U;
    }
    else
    {
        /* Get active partition table address as setup by Boot2 */
        act_part_id = !(!(g_boot_flags & BOOT_PARTITION_TABLE_MASK));
    }

    g_active_part_addr = part_get_table_addr_from_id(act_part_id);
    part_init_done     = true;
    return part_read_layout();
}

int part_write_layout(void)
{
    /* Get and update passive partition table */
    uint32_t part_addr = part_get_passive_table_addr();
    uint32_t crc;
    int write_size;

    mflash_drv_erase(part_addr, FL_PART_SIZE);

    /* Increment active partition table generation level this will get
     * written to passive address */
    ++g_flash_table.gen_level;

    /* Update checksum with updated generation level */
    crc               = crc32((uint8_t *)&g_flash_table, sizeof(struct partition_table) - 4, 0);
    g_flash_table.crc = crc;

    mflash_drv_write(part_addr, (uint32_t *)&g_flash_table, sizeof(struct partition_table));

    part_addr += sizeof(struct partition_table);

    if (g_flash_table.partition_entries_no > MAX_FL_COMP)
    {
        PRINTF(
            "Only %d partition entries are supported."
            "Truncating ...",
            MAX_FL_COMP);
        g_flash_table.partition_entries_no = MAX_FL_COMP;
    }

    write_size = g_flash_table.partition_entries_no * sizeof(struct partition_entry);
    mflash_drv_write(part_addr, (uint32_t *)g_flash_parts, write_size);
    part_addr += write_size;

    crc = crc32((uint8_t *)g_flash_parts, write_size, 0);
    mflash_drv_write(part_addr, (uint32_t *)&crc, sizeof(crc));

    return 0;
}

void part_to_flash_desc(struct partition_entry *p, flash_desc_t *f)
{
    f->fl_dev   = p->device;
    f->fl_start = p->start;
    f->fl_size  = p->size;
}

struct partition_entry *part_get_layout_by_id(enum flash_comp comp, short *start_index)
{
    int i = 0;

    if (start_index)
        i = *start_index;

    for (; i < g_flash_table.partition_entries_no; i++)
    {
        if (g_flash_parts[i].type == comp)
        {
            if (start_index)
                *start_index = i + 1;
            return &g_flash_parts[i];
        }
    }

    return NULL;
}

struct partition_entry *part_get_layout_by_name(const char *name, short *start_index)
{
    int i = 0;

    if (start_index)
        i = *start_index;

    for (; i < g_flash_table.partition_entries_no; i++)
    {
        if (!strcmp(g_flash_parts[i].name, name))
        {
            if (start_index)
                *start_index = i + 1;
            return &g_flash_parts[i];
        }
    }

    return NULL;
}

struct partition_entry *part_get_active_partition(struct partition_entry *p1, struct partition_entry *p2)
{
    /* Somehow there aren't two partition defined for this */
    if (!p1 || !p2)
        return NULL;

    return p1->gen_level >= p2->gen_level ? p1 : p2;
}

struct partition_entry *part_get_active_partition_by_name(const char *name)
{
    short index                = 0;
    struct partition_entry *p1 = part_get_layout_by_name(name, &index);
    struct partition_entry *p2 = part_get_layout_by_name(name, &index);

    return part_get_active_partition(p1, p2);
}

struct partition_entry *part_get_passive_partition(struct partition_entry *p1, struct partition_entry *p2)
{
    /* Somehow there aren't two partition defined for this */
    if (!p1 || !p2)
        return NULL;

    return p1->gen_level < p2->gen_level ? p1 : p2;
}

struct partition_entry *part_get_passive_partition_by_name(const char *name)
{
    short index                = 0;
    struct partition_entry *p1 = part_get_layout_by_name(name, &index);
    struct partition_entry *p2 = part_get_layout_by_name(name, &index);

    return part_get_passive_partition(p1, p2);
}

static bool part_match(struct partition_entry *p1, struct partition_entry *p2)
{
    if (p1->type == p2->type && p1->device == p2->device && p1->start == p2->start && p1->size == p2->size)
        return true;

    return false;
}

int part_set_active_partition(struct partition_entry *p)
{
    short index                = 0;
    struct partition_entry *p1 = part_get_layout_by_name(p->name, &index);
    struct partition_entry *p2 = part_get_layout_by_name(p->name, &index);

    /* Somehow there aren't two partition defined for this */
    if (!p1 || !p2)
        return -WM_E_INVAL;

    if (part_match(p1, p) == true)
        p1->gen_level = p2->gen_level + 1;
    else if (part_match(p2, p) == true)
        p2->gen_level = p1->gen_level + 1;
    else
        return -WM_E_INVAL;

    part_write_layout();
    return WM_SUCCESS;
}

bool part_is_flash_desc_within_one_partition(flash_desc_t *f)
{
    int i;
    uint32_t start = f->fl_start;
    uint32_t end   = f->fl_start + f->fl_size;

    if (start >= end)
    {
        return false;
    }

    for (i = 0; i < g_flash_table.partition_entries_no; i++)
    {
        if ((start >= g_flash_parts[i].start) && (end <= (g_flash_parts[i].start + g_flash_parts[i].size)))
        {
            return true;
        }
    }

    return false;
}
