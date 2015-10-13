/*
 * This file is part of the KeepKey project.
 *
 * Copyright (C) 2015 KeepKey LLC
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RESOURCES_H
#define RESOURCES_H

/* === Includes ============================================================ */

#include <stdint.h>
#include <stdbool.h>

/* === Typedefs ============================================================ */

typedef struct
{
    const void *(*get_image_data)(uint8_t *);
    uint16_t    width;
    uint16_t    height;
} Image;

/* Image frame information */
typedef struct
{
    const Image    *image;
    uint32_t        duration;
} AnimationFrame;

/* Image animation information */
typedef struct
{
    int                     length;
    const AnimationFrame   *frames;
} ImageAnimation;

/* === Functions =========================================================== */

const Image *get_confirm_icon_image(void);
const Image *get_confirmed_image(void);
const Image *get_unplug_image(void);
const Image *get_recovery_image(void);
const Image *get_warning_image(void);

#if HAVE_U2F

typedef struct
{
	const Image 	*image;
	uint8_t			appId[32];	
	char			commonName[20];
} U2fWellKnown;

const Image *get_ledger_logo_image(void);
const Image *get_github_logo_image(void);
const Image *get_google_logo_image(void);
const Image *get_dropbox_logo_image(void);
const U2fWellKnown *get_u2f_well_known(void);

#endif

const ImageAnimation *get_confirm_icon_animation(void);
const ImageAnimation *get_confirming_animation(void);
const ImageAnimation *get_loading_animation(void);
const ImageAnimation *get_warning_animation(void);
const ImageAnimation *get_logo_animation(void);
const ImageAnimation *get_logo_reversed_animation(void);

uint32_t get_image_animation_duration(const ImageAnimation *img_animation);
const Image *get_image_animation_frame(const ImageAnimation *img_animation,
                                       const uint32_t elapsed, bool loop);

#endif
