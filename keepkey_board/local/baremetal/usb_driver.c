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

/* === Includes ============================================================ */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/desig.h>
#include <libopencm3/usb/hid.h>
#include <libopencm3/stm32/rcc.h>

#include "keepkey_board.h"

/* === Private Variables =================================================== */

#define ENDPOINT_ADDRESS_U2F_IN   (0x83)
#define ENDPOINT_ADDRESS_U2F_OUT  (0x03)

static uint8_t usbd_control_buffer[USBD_CONTROL_BUFFER_SIZE];

/* USB Device state structure.  */
static usbd_device *usbd_dev = NULL;

/*
 * Used to track the initialization of the USB device.  Set to true after the
 * USB stack is configured.
 */
static bool usb_configured = false;

/* USB device descriptor */
static const struct usb_device_descriptor dev_descr = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = 0,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = USB_SEGMENT_SIZE,
	.idVendor = 0x2B24,   /* KeepKey Vendor ID */
	.idProduct = 0x0001,
	.bcdDevice = 0x0100,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/* Got via usbhid-dump from CP2110 */
static const uint8_t hid_report_descriptor[] = {
	0x06, 0x00, 0xFF, 0x09, 0x01, 0xA1, 0x01, 0x09, 0x01, 0x75, 0x08, 0x95, 0x40, 0x26, 0xFF, 0x00,
	0x15, 0x00, 0x85, 0x01, 0x95, 0x01, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x02,
	0x95, 0x02, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x03, 0x95, 0x03, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x04, 0x95, 0x04, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x05, 0x95, 0x05, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x06,
	0x95, 0x06, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x07, 0x95, 0x07, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x08, 0x95, 0x08, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x09, 0x95, 0x09, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0A,
	0x95, 0x0A, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0B, 0x95, 0x0B, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0C, 0x95, 0x0C, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x0D, 0x95, 0x0D, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0E,
	0x95, 0x0E, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x0F, 0x95, 0x0F, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x10, 0x95, 0x10, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x11, 0x95, 0x11, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x12,
	0x95, 0x12, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x13, 0x95, 0x13, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x14, 0x95, 0x14, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x15, 0x95, 0x15, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x16,
	0x95, 0x16, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x17, 0x95, 0x17, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x18, 0x95, 0x18, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x19, 0x95, 0x19, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1A,
	0x95, 0x1A, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1B, 0x95, 0x1B, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1C, 0x95, 0x1C, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x1D, 0x95, 0x1D, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1E,
	0x95, 0x1E, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x1F, 0x95, 0x1F, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x20, 0x95, 0x20, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x21, 0x95, 0x21, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x22,
	0x95, 0x22, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x23, 0x95, 0x23, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x24, 0x95, 0x24, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x25, 0x95, 0x25, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x26,
	0x95, 0x26, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x27, 0x95, 0x27, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x28, 0x95, 0x28, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x29, 0x95, 0x29, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2A,
	0x95, 0x2A, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2B, 0x95, 0x2B, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2C, 0x95, 0x2C, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x2D, 0x95, 0x2D, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2E,
	0x95, 0x2E, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x2F, 0x95, 0x2F, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x30, 0x95, 0x30, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x31, 0x95, 0x31, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x32,
	0x95, 0x32, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x33, 0x95, 0x33, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x34, 0x95, 0x34, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x35, 0x95, 0x35, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x36,
	0x95, 0x36, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x37, 0x95, 0x37, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x38, 0x95, 0x38, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x39, 0x95, 0x39, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3A,
	0x95, 0x3A, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3B, 0x95, 0x3B, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3C, 0x95, 0x3C, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01,
	0x91, 0x02, 0x85, 0x3D, 0x95, 0x3D, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3E,
	0x95, 0x3E, 0x09, 0x01, 0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x3F, 0x95, 0x3F, 0x09, 0x01,
	0x81, 0x02, 0x09, 0x01, 0x91, 0x02, 0x85, 0x40, 0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x41,
	0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x42, 0x95, 0x06, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x43,
	0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x44, 0x95, 0x02, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x45,
	0x95, 0x04, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x46, 0x95, 0x02, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x47,
	0x95, 0x02, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x50, 0x95, 0x08, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x51,
	0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x52, 0x95, 0x01, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x60,
	0x95, 0x0A, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x61, 0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x62,
	0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x63, 0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x64,
	0x95, 0x3F, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x65, 0x95, 0x3E, 0x09, 0x01, 0xB1, 0x02, 0x85, 0x66,
	0x95, 0x13, 0x09, 0x01, 0xB1, 0x02, 0xC0,
};

#if HAVE_U2F

// This grants u2fDevices permission on Chrome
// https://chromium.googlesource.com/chromium/src.git/+/667c5595a7326d7e57375afbd2be922dd3a8810f/extensions/browser/api/hid/hid_device_manager.cc#134 

static const uint8_t hid_report_descriptor_u2f[] = {
        0x06, 0xD0, 0xF1,       // Usage page (vendor defined) 
        0x09, 0x01,     // Usage ID (vendor defined)
        0xA1, 0x01,     // Collection (application)

                // The Input report
        0x09, 0x03,             // Usage ID - vendor defined
        0x15, 0x00,             // Logical Minimum (0)
        0x26, 0xFF, 0x00,   // Logical Maximum (255)
        0x75, 0x08,             // Report Size (255 bits)
        0x95, 0x40,           // Report Count (2 fields)
        0x81, 0x08,             // Input (Data, Variable, Absolute)  

                // The Output report
        0x09, 0x04,             // Usage ID - vendor defined
        0x15, 0x00,             // Logical Minimum (0)
        0x26, 0xFF, 0x00,   // Logical Maximum (255)
        0x75, 0x08,             // Report Size (255 bits)
        0x95, 0x40,           // Report Count (2 fields)
        0x91, 0x08,             // Output (Data, Variable, Absolute)  

        0xC0 // end
};

#endif

static const struct {
	struct usb_hid_descriptor hid_descriptor;
	struct {
		uint8_t bReportDescriptorType;
		uint16_t wDescriptorLength;
	} __attribute__((packed)) hid_report;
} __attribute__((packed)) hid_function = {
	.hid_descriptor = {
		.bLength = sizeof(hid_function),
		.bDescriptorType = USB_DT_HID,
		.bcdHID = 0x0111,
		.bCountryCode = 0,
		.bNumDescriptors = 1,
	},
	.hid_report = {
		.bReportDescriptorType = USB_DT_REPORT,
		.wDescriptorLength = sizeof(hid_report_descriptor),
	}
};

#if HAVE_U2F

static const struct {
        struct usb_hid_descriptor hid_descriptor;
        struct {
                uint8_t bReportDescriptorType;
                uint16_t wDescriptorLength;
        } __attribute__((packed)) hid_report_u2f;
} __attribute__((packed)) hid_function_u2f = {
        .hid_descriptor = {
                .bLength = sizeof(hid_function_u2f),
                .bDescriptorType = USB_DT_HID,
                .bcdHID = 0x0111,
                .bCountryCode = 0,
                .bNumDescriptors = 1,
        },
        .hid_report_u2f = {
                .bReportDescriptorType = USB_DT_REPORT,
                .wDescriptorLength = sizeof(hid_report_descriptor_u2f),
        }
};

#endif

static const struct usb_endpoint_descriptor hid_endpoints[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = ENDPOINT_ADDRESS_IN,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = USB_SEGMENT_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = ENDPOINT_ADDRESS_OUT,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = USB_SEGMENT_SIZE,
	.bInterval = 1,
}};

static const struct usb_interface_descriptor hid_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
	.endpoint = hid_endpoints,
	.extra = &hid_function,
	.extralen = sizeof(hid_function),
}};

#if DEBUG_LINK
static const struct usb_endpoint_descriptor hid_endpoints_debug[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = ENDPOINT_ADDRESS_DEBUG_IN,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = USB_SEGMENT_SIZE,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = ENDPOINT_ADDRESS_DEBUG_OUT,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = USB_SEGMENT_SIZE,
	.bInterval = 1,
}};

static const struct usb_interface_descriptor hid_iface_debug[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_HID,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
	.endpoint = hid_endpoints_debug,
	.extra = &hid_function,
	.extralen = sizeof(hid_function),
}};
#endif

#if HAVE_U2F
static const struct usb_endpoint_descriptor hid_endpoints_u2f[] = {{
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = ENDPOINT_ADDRESS_U2F_IN,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = USB_SEGMENT_SIZE,
        .bInterval = 1,
}, {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = ENDPOINT_ADDRESS_U2F_OUT,
        .bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
        .wMaxPacketSize = USB_SEGMENT_SIZE,
        .bInterval = 1,
}};

static const struct usb_interface_descriptor hid_iface_u2f[] = {{
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
#if DEBUG_LINK
        .bInterfaceNumber = 2,
#else
	.bInterfaceNumber = 1,
#endif
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_CLASS_HID,
        .bInterfaceSubClass = 0,
        .bInterfaceProtocol = 0,
        .iInterface = 0,
        .endpoint = hid_endpoints_u2f,
        .extra = &hid_function_u2f,
        .extralen = sizeof(hid_function_u2f),
}};
#endif

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = hid_iface,
#if DEBUG_LINK && HAVE_U2F
}, {
	.num_altsetting = 1,
	.altsetting = hid_iface_debug,
}, {
	.num_altsetting = 1,
	.altsetting = hid_iface_u2f
#elif HAVE_U2F
}, {
        .num_altsetting = 1,
        .altsetting = hid_iface_u2f,
#elif DEBUG_LINK
}, {
	.num_altsetting = 1,
	.altsetting = hid_iface_debug,
#endif
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
#if DEBUG_LINK && HAVE_U2F
	.bNumInterfaces = 3,
#elif DEBUG_LINK
	.bNumInterfaces = 2,
#elif HAVE_U2F
	.bNumInterfaces = 2,
#else
	.bNumInterfaces = 1,
#endif
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,
	.interface = ifaces,
};

static const char *usb_strings[] = {
	"KeepKey, LLC.",
	"KeepKey",
	""
};

/* === Variables =========================================================== */

/* This optional callback is configured by the user to handle receive events.  */
usb_rx_callback_t user_rx_callback = NULL;

#if DEBUG_LINK
usb_rx_callback_t user_debug_rx_callback = NULL;
#endif

#if HAVE_U2F
usb_rx_callback_t u2f_rx_callback = NULL;
#endif

/* === Private Functions =================================================== */

static int hid_control_request(usbd_device *dev, struct usb_setup_data *req, uint8_t **buf, uint16_t *len,
			void (**complete)(usbd_device *, struct usb_setup_data *))
{
	(void)complete;
	(void)dev;

	if ((req->bmRequestType != ENDPOINT_ADDRESS_IN) ||
	    (req->bRequest != USB_REQ_GET_DESCRIPTOR) ||
	    (req->wValue != 0x2200))
		return 0;

#if HAVE_U2F == 0
	/* Handle the HID report descriptor. */
	*buf = (uint8_t *)hid_report_descriptor;
	*len = sizeof(hid_report_descriptor);
#else
#if HAVE_U2F && DEBUG_LINK
	if (req->wIndex < 2) {
#else
	if (!req->wIndex) {
#endif
	        *buf = (uint8_t *)hid_report_descriptor;
       	 	*len = sizeof(hid_report_descriptor);
	}
	else {
	        *buf = (uint8_t *)hid_report_descriptor_u2f;
       		*len = sizeof(hid_report_descriptor_u2f);
	}
#endif
	return 1;
}

/*
 * hid_rx_callback() - Callback function to process received packet from USB host
 *
 * INPUT 
 *     - dev: pointer to USB device handler
 *     - ep: unused 
 * OUTPUT 
 *     none
 *
 */
static void hid_rx_callback(usbd_device *dev, uint8_t ep)
{
    (void)ep;

    /* Receive into the message buffer. */
    UsbMessage m;
    uint16_t rx = usbd_ep_read_packet(dev, 
                                      ENDPOINT_ADDRESS_OUT, 
                                      m.message, 
                                      USB_SEGMENT_SIZE);

    if(rx && user_rx_callback)
    {
        m.len = rx;
        user_rx_callback(&m);
    }
}

/*
 * hid_debug_rx_callback() - Callback function to process received packet from USB host on debug endpoint
 *
 * INPUT
 *     - dev: pointer to USB device handler
 *     - ep: unused
 * OUTPUT
 *     none
 *
 */
#if DEBUG_LINK
static void hid_debug_rx_callback(usbd_device *dev, uint8_t ep)
{
    (void)ep;

    /* Receive into the message buffer. */
    UsbMessage m;
    uint16_t rx = usbd_ep_read_packet(dev,
                                      ENDPOINT_ADDRESS_DEBUG_OUT,
                                      m.message,
                                      USB_SEGMENT_SIZE);

    if(rx && user_debug_rx_callback)
    {
        m.len = rx;
        user_debug_rx_callback(&m);
    }
}
#endif

#if HAVE_U2F
static void hid_u2f_rx_callback(usbd_device *dev, uint8_t ep)
{
    (void)ep;

    /* Receive into the message buffer. */
    UsbMessage m;
    uint16_t rx = usbd_ep_read_packet(dev,
                                      ENDPOINT_ADDRESS_U2F_OUT,
                                      m.message,
                                      USB_SEGMENT_SIZE);

    if(rx && u2f_rx_callback)
    {
        m.len = rx;
        u2f_rx_callback(&m);
    }
}
#endif

/*
 * hid_set_config_callback() - Config USB IN/OUT endpoints and register callbacks
 *
 * INPUT -
 *     - dev: pointer to USB device handler
 *     - wValue: not used 
 * OUTPUT -
 *      none
 */
static void hid_set_config_callback(usbd_device *dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(dev, ENDPOINT_ADDRESS_IN,  USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, 0);
	usbd_ep_setup(dev, ENDPOINT_ADDRESS_OUT, USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, hid_rx_callback);
#if DEBUG_LINK
	usbd_ep_setup(dev, ENDPOINT_ADDRESS_DEBUG_IN,  USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, 0);
	usbd_ep_setup(dev, ENDPOINT_ADDRESS_DEBUG_OUT, USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, hid_debug_rx_callback);
#endif
#if HAVE_U2F
        usbd_ep_setup(dev, ENDPOINT_ADDRESS_U2F_IN,  USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, 0);
        usbd_ep_setup(dev, ENDPOINT_ADDRESS_U2F_OUT, USB_ENDPOINT_ATTR_INTERRUPT, USB_SEGMENT_SIZE, hid_u2f_rx_callback);
#endif

	usbd_register_control_callback(
		dev,
		USB_REQ_TYPE_STANDARD | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		hid_control_request);

        usb_configured = true;
}

/*
 * usb_tx_helper() - Common way to transmit USB message to host 
 *
 * INPUT
 *     - message: pointer message buffer
 *     - len: length of message
 *     - endpoint: endpoint for transmission
 * OUTPUT
 *     true/false
 */
static bool usb_tx_helper(uint8_t *message, uint32_t len, uint8_t endpoint)
{
    uint32_t pos = 1;

    /* Chunk out message */
    while(pos < len)
    {
        uint8_t tmp_buffer[USB_SEGMENT_SIZE] = { 0 };

        tmp_buffer[0] = '?';
        memcpy(tmp_buffer + 1, message + pos, USB_SEGMENT_SIZE - 1);

        while(usbd_ep_write_packet(usbd_dev, endpoint, tmp_buffer, USB_SEGMENT_SIZE) == 0) {};

        pos += USB_SEGMENT_SIZE - 1;
    }

    return(true);
}

#if HAVE_U2F

static bool usb_tx_helper_raw(uint8_t *message, uint32_t len, uint8_t endpoint) 
{
    uint32_t pos = 0;

    /* Chunk out message */
    while(pos < len)
    {
        uint8_t tmp_buffer[USB_SEGMENT_SIZE] = { 0 };

        memcpy(tmp_buffer, message + pos, USB_SEGMENT_SIZE);

        while(usbd_ep_write_packet(usbd_dev, endpoint, tmp_buffer, USB_SEGMENT_SIZE) == 0) {};

        pos += USB_SEGMENT_SIZE;
    }

    return(true);
}

#endif

/* === Functions =========================================================== */

/*
 * usb_init() - Initialize USB registers and set callback functions 
 *
 * INPUT
 *     none
 * OUTPUT 
 *     true/false status of USB init
 */
bool usb_init(void)
{
    bool ret_stat = true;

    /* Skip initialization if alrealy initialized */
    if(usbd_dev == NULL) {
        gpio_mode_setup(USB_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, USB_GPIO_PORT_PINS);
        gpio_set_af(USB_GPIO_PORT, GPIO_AF10, USB_GPIO_PORT_PINS);

        static char serial_number[100];
        desig_get_unique_id_as_string(serial_number, sizeof(serial_number));
        usb_strings[NUM_USB_STRINGS-1] = serial_number;
        usbd_dev = usbd_init(&otgfs_usb_driver, 
                         &dev_descr, 
                         &config, 
                         usb_strings,
                         NUM_USB_STRINGS, 
                         usbd_control_buffer, 
                         sizeof(usbd_control_buffer));
        if(usbd_dev != NULL) {
            usbd_register_set_config_callback(usbd_dev, hid_set_config_callback);
        } else {
            /* error: unable init usbd_dev */
            ret_stat = false;
        }
    }
    
    return (ret_stat);
}

/*
 * usb_poll() - Poll USB port for message
 *  
 * INPUT
 *     none
 * OUTPUT
 *     none
 */
void usb_poll(void)
{
    usbd_poll(usbd_dev);
}

/*
 * usb_tx() - Transmit USB message to host via normal endpoint
 *
 * INPUT
 *     - message: pointer message buffer
 *     - len: length of message
 * OUTPUT
 *     true/false
 */
bool usb_tx(uint8_t *message, uint32_t len)
{
    return usb_tx_helper(message, len, ENDPOINT_ADDRESS_IN);
}

/*
 * usb_debug_tx() - Transmit usb message to host via debug endpoint
 *
 * INPUT
 *     - message: pointer message buffer
 *     - len: length of message
 * OUTPUT
 *     true/false
 */
#if DEBUG_LINK
bool usb_debug_tx(uint8_t *message, uint32_t len)
{
    return usb_tx_helper(message, len, ENDPOINT_ADDRESS_DEBUG_IN);
}
#endif

#if HAVE_U2F
bool usb_u2f_tx(uint8_t *message, uint32_t len) 
{
    return usb_tx_helper_raw(message, len, ENDPOINT_ADDRESS_U2F_IN);
}
#endif

/*
 * usb_set_rx_callback() - Setup USB receive callback function pointer
 *
 * INPUT
 *     - callback: callback function
 * OUTPUT
 *     none
 */
void usb_set_rx_callback(usb_rx_callback_t callback)
{
    user_rx_callback = callback;
}

/*
 * usb_set_debug_rx_callback() - Setup USB receive callback function pointer for debug link
 *
 * INPUT
 *     - callback: callback function
 * OUTPUT
 *     none
 */
#if DEBUG_LINK
void usb_set_debug_rx_callback(usb_rx_callback_t callback)
{
    user_debug_rx_callback = callback;
}
#endif

#if HAVE_U2F
void usb_set_u2f_rx_callback(usb_rx_callback_t callback)
{
    u2f_rx_callback = callback;
}
#endif

/*
 * get_usb_init_stat() - Get USB initialization status
 *
 * INPUT
 *     none
 * OUTPUT
 *     USB pointer 
 */
usbd_device *get_usb_init_stat(void)
{
    return(usbd_dev);
}
