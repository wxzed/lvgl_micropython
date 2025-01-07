import time


# Sensecap Indicator
def init(self):
    param_buf = bytearray(16)
    param_mv = memoryview(param_buf)

    param_buf[:5] = bytearray([0x77, 0x01, 0x00, 0x00, 0x10])
    self.set_params(0xFF, param_mv[:5])

    param_buf[:2] = bytearray([0x3B, 0x00])
    self.set_params(0xC0, param_mv[:2])

    param_buf[:2] = bytearray([0x0D, 0x02])
    self.set_params(0xC1, param_mv[:2])

    param_buf[:2] = bytearray([0x31, 0x05])
    self.set_params(0xC2, param_mv[:2])

    param_buf[:1] = bytearray([0x04])
    self.set_params(0xC7, param_mv[:1])

    # changed by manufacture
    param_buf[0] = 0x08
    self.set_params(0xCD, param_mv[:1])

    # Positive Voltage Gamma Control
    param_buf[:16] = bytearray([
        0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08,
        0x07, 0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18])
    self.set_params(0xB0, param_mv[:16])

    # Negative Voltage Gamma Control
    param_buf[:16] = bytearray([
        0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08,
        0x08, 0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18])
    self.set_params(0xB1, param_mv[:16])

    # PAGE1
    param_buf[:5] = bytearray([0x77, 0x01, 0x00, 0x00, 0x11])
    self.set_params(0xFF, param_mv[:5])

    # Vop=4.7375v
    param_buf[0] = 0x60
    self.set_params(0xB0, param_mv[:1])

    # VCOM=32
    param_buf[0] = 0x32
    self.set_params(0xB1, param_mv[:1])

    # VGH=15v
    param_buf[0] = 0x07
    self.set_params(0xB2, param_mv[:1])

    param_buf[0] = 0x80
    self.set_params(0xB3, param_mv[:1])

    # VGL=-10.17v
    param_buf[0] = 0x49
    self.set_params(0xB5, param_mv[:1])

    param_buf[0] = 0x85
    self.set_params(0xB7, param_mv[:1])

    # AVDD=6.6 & AVCL=-4.6
    param_buf[0] = 0x21
    self.set_params(0xB8, param_mv[:1])

    param_buf[0] = 0x78
    self.set_params(0xC1, param_mv[:1])

    param_buf[0] = 0x78
    self.set_params(0xC2, param_mv[:1])

    time.sleep_ms(20)

    param_buf[:3] = bytearray([0x00, 0x1B, 0x02])
    self.set_params(0xE0, param_mv[:3])

    param_buf[:11] = bytearray([
        0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0,
        0x00, 0x00, 0x00, 0x44, 0x44])
    self.set_params(0xE1, param_mv[:11])

    param_buf[:12] = bytearray([
        0x11, 0x11, 0x44, 0x44, 0xED, 0xA0,
        0x00, 0x00, 0xEC, 0xA0, 0x00, 0x00])
    self.set_params(0xE2, param_mv[:12])

    param_buf[:4] = bytearray([0x00, 0x00, 0x11, 0x11])
    self.set_params(0xE3, param_mv[:4])

    param_buf[:2] = bytearray([0x44, 0x44])
    self.set_params(0xE4, param_mv[:2])

    param_buf[:16] = bytearray([
        0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0,
        0x0E, 0xED, 0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0])
    self.set_params(0xE5, param_mv[:16])

    param_buf[:4] = bytearray([0x00, 0x00, 0x11, 0x11])
    self.set_params(0xE6, param_mv[:4])

    param_buf[:2] = bytearray([0x44, 0x44])
    self.set_params(0xE7, param_mv[:2])

    param_buf[:16] = bytearray([
        0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0,
        0x0D, 0xEC, 0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0])
    self.set_params(0xE8, param_mv[:16])

    param_buf[:7] = bytearray([0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40])
    self.set_params(0xEB, param_mv[:7])

    param_buf[:2] = bytearray([0x3C, 0x00])
    self.set_params(0xEC, param_mv[:2])

    param_buf[:16] = bytearray([
        0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA])
    self.set_params(0xED, param_mv[:16])

    param_buf[:1] = bytearray([0x10])
    self.set_params(0x36, param_mv[:1])

    # -----------VAP & VAN---------------
    param_buf[:5] = bytearray([0x77, 0x01, 0x00, 0x00, 0x13])
    self.set_params(0xFF, param_mv[:5])

    param_buf[0] = 0xE4
    self.set_params(0xE5, param_mv[:1])

    param_buf[:5] = bytearray([0x77, 0x01, 0x00, 0x00, 0x00])
    self.set_params(0xFF, param_mv[:5])

    # 0x70 RGB888, 0x60 RGB666, 0x50 RGB565
    param_buf[0] = 0x60
    self.set_params(0x3A, param_mv[:1])

    # Display Inversion On
    self.set_params(0x21)

    # Sleep Out
    self.set_params(0x11)

    time.sleep_ms(120)  # NOQA

    # Display On
    self.set_params(0x29)

    time.sleep_ms(120)  # NOQA
