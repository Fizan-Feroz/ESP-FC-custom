import sys
import os
import pcbnew

def build_board():
    print("Step 0: Creating board...")
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(4)
    
    cx, cy = 100.0, 100.0
    half_size = 18.0
    corner_r = 3.0
    x_min, x_max = cx - half_size, cx + half_size
    y_min, y_max = cy - half_size, cy + half_size
    
    def mm(v):
        return pcbnew.FromMM(v)
    
    print("Step 1: Outline...")
    edge_layer = pcbnew.Edge_Cuts
    rect = pcbnew.PCB_SHAPE(board)
    rect.SetShape(pcbnew.SHAPE_T_RECT)
    rect.SetLayer(edge_layer)
    rect.SetStart(pcbnew.VECTOR2I(mm(cx - half_size), mm(cy - half_size)))
    rect.SetEnd(pcbnew.VECTOR2I(mm(cx + half_size), mm(cy + half_size)))
    rect.SetCornerRadius(mm(corner_r))
    rect.SetWidth(mm(0.15))
    board.Add(rect)

    print("Step 2: Nets...")
    net_names = [
        "GND", "VBAT", "+5V", "+3V3",
        "MOTOR_1", "MOTOR_2", "MOTOR_3", "MOTOR_4",
        "SPI_SCK", "SPI_MOSI", "SPI_MISO", "SPI_CS0", "GYRO_INT",
        "I2C_SCL", "I2C_SDA",
        "UART0_TX", "UART0_RX",
        "UART1_TX", "UART1_RX",
        "UART2_TX", "UART2_RX",
        "VBAT_ADC", "CURR_ADC",
        "BUZZER", "BUZZER-", "LED_STAT",
        "CHIP_PU", "BOOT"
    ]
    nets = {name: pcbnew.NETINFO_ITEM(board, name, i) for i, name in enumerate(net_names, 1)}

    print("Step 3: Footprints...")
    fp_base = "C:/Program Files/KiCad/10.0/share/kicad/footprints/"
    fps = {}
    
    def add_fp(lib, name, ref, val, x, y, rot_deg=0, layer=pcbnew.F_Cu):
        fp = pcbnew.FootprintLoad(fp_base + lib, name)
        if not fp:
            print(f"Failed to load {lib}:{name}")
            return None
        fp.SetReference(ref)
        fp.SetValue(val)
        fp.SetPosition(pcbnew.VECTOR2I(mm(x), mm(y)))
        fp.SetOrientationDegrees(rot_deg)
        board.Add(fp)
        if layer != pcbnew.F_Cu:
            fp.Flip(pcbnew.VECTOR2I(mm(x), mm(y)), True)
        fps[ref] = fp
        return fp

    h_dist = 15.25
    add_fp("MountingHole.pretty", "MountingHole_3.2mm_M3_Pad_Via", "H1", "M3_Mount", cx - h_dist, cy - h_dist)
    add_fp("MountingHole.pretty", "MountingHole_3.2mm_M3_Pad_Via", "H2", "M3_Mount", cx + h_dist, cy - h_dist)
    add_fp("MountingHole.pretty", "MountingHole_3.2mm_M3_Pad_Via", "H3", "M3_Mount", cx + h_dist, cy + h_dist)
    add_fp("MountingHole.pretty", "MountingHole_3.2mm_M3_Pad_Via", "H4", "M3_Mount", cx - h_dist, cy + h_dist)

    add_fp("RF_Module.pretty", "ESP32-WROOM-32E", "U1", "ESP32-WROOM-32E", cx, cy - 4.5, rot_deg=0)
    add_fp("Sensor_Motion.pretty", "InvenSense_QFN-24_4x4mm_P0.5mm", "U2", "MPU-6000/6500", cx, cy + 9.0, rot_deg=0)
    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x08_P2.54mm_Vertical", "J_IMU", "IMU_Breakout", cx - 11.5, cy + 8.5, rot_deg=90)
    add_fp("Connector_JST.pretty", "JST_SH_SM08B-SRSS-TB_1x08-1MP_P1.00mm_Horizontal", "J_ESC", "4-IN-1_ESC", cx, cy + 15.5, rot_deg=180)
    add_fp("Package_SO.pretty", "SOIC-16_3.9x9.9mm_P1.27mm", "U3", "CH340C", cx - 8.0, cy - 2.0, rot_deg=90, layer=pcbnew.B_Cu)

    add_fp("Package_TO_SOT_SMD.pretty", "SOT-23-6", "U4", "SY8120B_5V2A", cx + 8.5, cy + 4.5, rot_deg=0)
    add_fp("Inductor_SMD.pretty", "L_0805_2012Metric", "L1", "4.7uH", cx + 8.5, cy + 7.5, rot_deg=90)
    add_fp("Capacitor_SMD.pretty", "C_0805_2012Metric", "C_IN1", "10uF_50V", cx + 8.5, cy + 2.0)
    add_fp("Capacitor_SMD.pretty", "C_0805_2012Metric", "C_OUT1", "22uF_10V", cx + 8.5, cy + 10.0)

    add_fp("Package_TO_SOT_SMD.pretty", "SOT-23-5", "U5", "ME6211C33", cx + 11.5, cy - 4.5, rot_deg=0)
    add_fp("Capacitor_SMD.pretty", "C_0603_1608Metric", "C_3V3_1", "10uF", cx + 11.5, cy - 2.0)
    add_fp("Capacitor_SMD.pretty", "C_0603_1608Metric", "C_3V3_2", "100nF", cx + 11.5, cy - 7.0)

    add_fp("Button_Switch_SMD.pretty", "SW_Push_1P1T_NO_CK_KMR2", "SW_RST", "RESET", cx - 14.5, cy - 6.0, rot_deg=90)
    add_fp("Button_Switch_SMD.pretty", "SW_Push_1P1T_NO_CK_KMR2", "SW_BOOT", "BOOT", cx - 14.5, cy - 1.0, rot_deg=90)
    add_fp("LED_SMD.pretty", "LED_0603_1608Metric", "D1", "LED_PWR_RED", cx + 14.5, cy - 10.0, rot_deg=0)
    add_fp("LED_SMD.pretty", "LED_0603_1608Metric", "D2", "LED_STAT_GRN", cx + 14.5, cy - 7.5, rot_deg=0)
    add_fp("Resistor_SMD.pretty", "R_0603_1608Metric", "R_LED1", "1k", cx + 12.5, cy - 10.0)
    add_fp("Resistor_SMD.pretty", "R_0603_1608Metric", "R_LED2", "1k", cx + 12.5, cy - 7.5)

    add_fp("Resistor_SMD.pretty", "R_0603_1608Metric", "R_BAT1", "100k_1%", cx + 5.5, cy + 2.0)
    add_fp("Resistor_SMD.pretty", "R_0603_1608Metric", "R_BAT2", "10k_1%", cx + 5.5, cy + 4.0)

    add_fp("Package_TO_SOT_SMD.pretty", "SOT-23", "Q_BUZ", "2N7002", cx + 12.5, cy + 8.5)
    add_fp("Package_TO_SOT_SMD.pretty", "SOT-23", "D_BUZ", "BAT54", cx + 12.5, cy + 11.5)

    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x04_P2.54mm_Vertical", "J_RC", "ELRS_CRSF", cx - 14.5, cy + 5.0, rot_deg=90)
    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x04_P2.54mm_Vertical", "J_GPS", "GPS_TELEM", cx + 5.5, cy + 15.0, rot_deg=0)
    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x04_P2.54mm_Vertical", "J_I2C", "I2C_BARO_MAG", cx - 5.5, cy + 15.0, rot_deg=0)
    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x02_P2.54mm_Vertical", "J_BUZ", "BUZZER", cx + 14.5, cy + 5.0, rot_deg=90)
    add_fp("Connector_PinHeader_2.54mm.pretty", "PinHeader_1x02_P2.54mm_Vertical", "J_BAT", "VBAT_2S-6S", cx - 14.5, cy + 12.0, rot_deg=90)

    print("Step 4: Bind pads...")
    def bind_pad(ref, pad_num, net_name):
        fp = fps.get(ref)
        if not fp:
            return
        pad = fp.FindPadByNumber(str(pad_num))
        if pad and net_name in nets:
            pad.SetNet(nets[net_name])

    for h in ["H1", "H2", "H3", "H4"]:
        bind_pad(h, 1, "GND")

    esp_map = {
        1: "GND", 2: "+3V3", 3: "CHIP_PU",
        4: "VBAT_ADC", 5: "CURR_ADC", 6: "GYRO_INT",
        8: "UART1_RX", 9: "UART1_TX",
        10: "MOTOR_2", 11: "MOTOR_1", 12: "BUZZER", 13: "MOTOR_4", 14: "GND",
        23: "LED_STAT", 24: "BOOT", 25: "MOTOR_3",
        26: "UART2_RX", 27: "UART2_TX", 28: "SPI_CS0",
        29: "SPI_SCK", 30: "SPI_MISO", 32: "I2C_SDA",
        33: "UART0_RX", 34: "UART0_TX", 35: "I2C_SCL", 36: "SPI_MOSI",
        38: "GND"
    }
    for p_num, n_name in esp_map.items():
        bind_pad("U1", p_num, n_name)
    for pad in fps["U1"].Pads():
        if pad.GetNumber() not in [str(k) for k in esp_map.keys()]:
            pad.SetNet(nets["GND"])

    mpu_map = {
        1: "GND", 8: "+3V3", 9: "SPI_MISO", 11: "GND",
        12: "GYRO_INT", 13: "+3V3", 18: "GND",
        22: "SPI_CS0", 23: "SPI_SCK", 24: "SPI_MOSI"
    }
    for p_num, n_name in mpu_map.items():
        bind_pad("U2", p_num, n_name)

    esc_map = {
        1: "GND", 2: "VBAT", 3: "MOTOR_1", 4: "MOTOR_2",
        5: "MOTOR_3", 6: "MOTOR_4", 7: "CURR_ADC", 8: "UART2_RX"
    }
    for p_num, n_name in esc_map.items():
        bind_pad("J_ESC", p_num, n_name)

    imu_hdr_map = {
        1: "+3V3", 2: "GND", 3: "SPI_SCK", 4: "SPI_MOSI",
        5: "SPI_MISO", 6: "SPI_CS0", 7: "GYRO_INT", 8: "+3V3"
    }
    for p_num, n_name in imu_hdr_map.items():
        bind_pad("J_IMU", p_num, n_name)

    rc_map = {1: "+5V", 2: "GND", 3: "UART1_RX", 4: "UART1_TX"}
    for p_num, n_name in rc_map.items():
        bind_pad("J_RC", p_num, n_name)

    gps_map = {1: "+5V", 2: "GND", 3: "UART2_RX", 4: "UART2_TX"}
    for p_num, n_name in gps_map.items():
        bind_pad("J_GPS", p_num, n_name)

    i2c_map = {1: "+3V3", 2: "GND", 3: "I2C_SCL", 4: "I2C_SDA"}
    for p_num, n_name in i2c_map.items():
        bind_pad("J_I2C", p_num, n_name)

    bind_pad("J_BAT", 1, "VBAT")
    bind_pad("J_BAT", 2, "GND")
    bind_pad("J_BUZ", 1, "+5V")
    bind_pad("J_BUZ", 2, "BUZZER-")

    bind_pad("U4", 1, "VBAT")
    bind_pad("U4", 2, "GND")
    bind_pad("U4", 6, "+5V")
    bind_pad("C_IN1", 1, "VBAT")
    bind_pad("C_IN1", 2, "GND")
    bind_pad("L1", 1, "+5V")
    bind_pad("L1", 2, "+5V")
    bind_pad("C_OUT1", 1, "+5V")
    bind_pad("C_OUT1", 2, "GND")
    
    bind_pad("U5", 1, "+5V")
    bind_pad("U5", 2, "GND")
    bind_pad("U5", 5, "+3V3")
    bind_pad("C_3V3_1", 1, "+3V3")
    bind_pad("C_3V3_1", 2, "GND")
    bind_pad("C_3V3_2", 1, "+3V3")
    bind_pad("C_3V3_2", 2, "GND")

    bind_pad("R_BAT1", 1, "VBAT")
    bind_pad("R_BAT1", 2, "VBAT_ADC")
    bind_pad("R_BAT2", 1, "VBAT_ADC")
    bind_pad("R_BAT2", 2, "GND")

    bind_pad("Q_BUZ", 1, "BUZZER")
    bind_pad("Q_BUZ", 2, "GND")
    bind_pad("Q_BUZ", 3, "BUZZER-")

    print("Step 5: Routes...")
    def route_pair(ref1, p1, ref2, p2, net_name, width_mm, layer=pcbnew.F_Cu):
        fp1, fp2 = fps.get(ref1), fps.get(ref2)
        if not fp1 or not fp2:
            return
        pad1, pad2 = fp1.FindPadByNumber(str(p1)), fp2.FindPadByNumber(str(p2))
        if not pad1 or not pad2:
            return
        track = pcbnew.PCB_TRACK(board)
        track.SetStart(pad1.GetPosition())
        track.SetEnd(pad2.GetPosition())
        track.SetWidth(mm(width_mm))
        track.SetLayer(layer)
        board.Add(track)
        if net_name in nets:
            track.SetNet(nets[net_name])

    route_pair("U1", 11, "J_ESC", 3, "MOTOR_1", 0.35)
    route_pair("U1", 10, "J_ESC", 4, "MOTOR_2", 0.35)
    route_pair("U1", 25, "J_ESC", 5, "MOTOR_3", 0.35)
    route_pair("U1", 13, "J_ESC", 6, "MOTOR_4", 0.35)

    route_pair("U1", 29, "U2", 23, "SPI_SCK", 0.25)
    route_pair("U1", 36, "U2", 24, "SPI_MOSI", 0.25)
    route_pair("U1", 30, "U2", 9,  "SPI_MISO", 0.25)
    route_pair("U1", 28, "U2", 22, "SPI_CS0", 0.25)
    route_pair("U1", 6,  "U2", 12, "GYRO_INT", 0.25)
    
    route_pair("U2", 23, "J_IMU", 3, "SPI_SCK", 0.25)
    route_pair("U2", 24, "J_IMU", 4, "SPI_MOSI", 0.25)
    route_pair("U2", 9,  "J_IMU", 5, "SPI_MISO", 0.25)
    route_pair("U2", 22, "J_IMU", 6, "SPI_CS0", 0.25)
    route_pair("U2", 12, "J_IMU", 7, "GYRO_INT", 0.25)

    route_pair("U1", 35, "J_I2C", 3, "I2C_SCL", 0.25)
    route_pair("U1", 32, "J_I2C", 4, "I2C_SDA", 0.25)

    route_pair("U1", 8, "J_RC", 3, "UART1_RX", 0.25)
    route_pair("U1", 9, "J_RC", 4, "UART1_TX", 0.25)
    route_pair("U1", 26, "J_GPS", 3, "UART2_RX", 0.25)
    route_pair("U1", 27, "J_GPS", 4, "UART2_TX", 0.25)

    route_pair("J_BAT", 1, "C_IN1", 1, "VBAT", 0.8)
    route_pair("C_IN1", 1, "U4", 1, "VBAT", 0.8)
    route_pair("U4", 6, "L1", 1, "+5V", 0.8)
    route_pair("L1", 2, "C_OUT1", 1, "+5V", 0.8)
    route_pair("C_OUT1", 1, "U5", 1, "+5V", 0.6)
    route_pair("C_OUT1", 1, "J_RC", 1, "+5V", 0.6)
    route_pair("C_OUT1", 1, "J_GPS", 1, "+5V", 0.6)
    route_pair("C_OUT1", 1, "J_BUZ", 1, "+5V", 0.6)
    route_pair("U5", 5, "C_3V3_1", 1, "+3V3", 0.6)
    route_pair("C_3V3_1", 1, "U1", 2, "+3V3", 0.6)
    route_pair("C_3V3_1", 1, "U2", 13, "+3V3", 0.4)
    route_pair("U2", 13, "U2", 8, "+3V3", 0.3)
    route_pair("C_3V3_1", 1, "J_I2C", 1, "+3V3", 0.4)

    route_pair("R_BAT1", 2, "R_BAT2", 1, "VBAT_ADC", 0.25)
    route_pair("R_BAT1", 2, "U1", 4, "VBAT_ADC", 0.25)
    route_pair("J_ESC", 7, "U1", 5, "CURR_ADC", 0.25)

    route_pair("U1", 12, "Q_BUZ", 1, "BUZZER", 0.25)
    route_pair("Q_BUZ", 3, "J_BUZ", 2, "BUZZER-", 0.4)

    print("Step 6: Zones...")
    zone_gnd = pcbnew.ZONE(board)
    zone_gnd.SetLayer(pcbnew.In1_Cu)
    board.Add(zone_gnd)
    zone_gnd.SetNet(nets["GND"])
    poly_gnd = pcbnew.SHAPE_LINE_CHAIN()
    poly_gnd.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_min + 0.5)))
    poly_gnd.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_min + 0.5)))
    poly_gnd.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_max - 0.5)))
    poly_gnd.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_max - 0.5)))
    poly_gnd.SetClosed(True)
    zone_gnd.AddPolygon(poly_gnd)

    zone_pwr = pcbnew.ZONE(board)
    zone_pwr.SetLayer(pcbnew.In2_Cu)
    board.Add(zone_pwr)
    zone_pwr.SetNet(nets["+5V"])
    poly_pwr = pcbnew.SHAPE_LINE_CHAIN()
    poly_pwr.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_min + 0.5)))
    poly_pwr.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_min + 0.5)))
    poly_pwr.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_max - 0.5)))
    poly_pwr.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_max - 0.5)))
    poly_pwr.SetClosed(True)
    zone_pwr.AddPolygon(poly_pwr)

    zone_bgnd = pcbnew.ZONE(board)
    zone_bgnd.SetLayer(pcbnew.B_Cu)
    board.Add(zone_bgnd)
    zone_bgnd.SetNet(nets["GND"])
    poly_bgnd = pcbnew.SHAPE_LINE_CHAIN()
    poly_bgnd.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_min + 0.5)))
    poly_bgnd.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_min + 0.5)))
    poly_bgnd.Append(pcbnew.VECTOR2I(mm(x_max - 0.5), mm(y_max - 0.5)))
    poly_bgnd.Append(pcbnew.VECTOR2I(mm(x_min + 0.5), mm(y_max - 0.5)))
    poly_bgnd.SetClosed(True)
    zone_bgnd.AddPolygon(poly_bgnd)

    print("Step 7: Filling zones...")
    filler = pcbnew.ZONE_FILLER(board)
    filler.Fill(board.Zones())

    print("Step 8: Silkscreen...")
    texts = [
        ("ESP-FC v1.0", cx, cy - 14.0, 1.2, pcbnew.F_SilkS),
        ("ESP32-WROOM-32E", cx, cy - 12.0, 0.8, pcbnew.F_SilkS),
        ("M1 (GPIO 27)", cx + 10.5, cy - 14.5, 0.7, pcbnew.F_SilkS),
        ("M2 (GPIO 25)", cx + 10.5, cy + 14.5, 0.7, pcbnew.F_SilkS),
        ("M3 (GPIO 4)",  cx - 10.5, cy + 14.5, 0.7, pcbnew.F_SilkS),
        ("M4 (GPIO 12)", cx - 10.5, cy - 14.5, 0.7, pcbnew.F_SilkS),
        ("FRONT ^", cx, cy + 12.0, 1.0, pcbnew.F_SilkS),
        ("30.5x30.5 M3", cx, cy + 14.0, 0.8, pcbnew.F_SilkS),
    ]
    for txt, tx, ty, tsize, tlayer in texts:
        t = pcbnew.PCB_TEXT(board)
        t.SetText(txt)
        t.SetPosition(pcbnew.VECTOR2I(mm(tx), mm(ty)))
        t.SetLayer(tlayer)
        t.SetTextSize(pcbnew.VECTOR2I(mm(tsize), mm(tsize)))
        t.SetTextThickness(mm(tsize * 0.15))
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_CENTER)
        board.Add(t)

    print("Step 9: Saving board...")
    output_path = "c:/Users/fizan/OneDrive/Documents/ESP-FC/pcb/esp32-fc.kicad_pcb"
    saved = pcbnew.SaveBoard(output_path, board)
    print("PCB with full connections saved successfully:", saved)
    print("Nets count:", board.GetNetCount())
    print("Tracks count:", len(board.GetTracks()))
    print("Zones count:", len(board.Zones()))
    return saved

if __name__ == "__main__":
    build_board()
