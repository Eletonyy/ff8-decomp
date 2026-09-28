/**
 * @file menusav.h
 * @brief Public symbols of the menusav overlay (@c src/menu/menusav): its entries in
 *        menumain's screen table (@c D_801F7E6C, overlay 11).
 */
#ifndef MENUSAV_H
#define MENUSAV_H

#include "common.h"

extern void func_801E7190(void); /**< Save-point save menu. */
extern void func_801E7268(void); /**< Title menu: NEW GAME / Continue (screen 0x10). */
extern void func_801EAD28(void);

#endif /* MENUSAV_H */
