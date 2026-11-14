// ==============================================================================
// ĐỒ ÁN TỐT NGHIỆP: HỆ THỐNG BMS THÔNG MINH - EDGE AI & IOT GATEWAY
// THIẾT KẾ VỎ SẢN PHẨM HOÀN CHỈNH (PARAMETRIC ENCLOSURE 3D MODEL)
// ==============================================================================
// Tương thích: OpenSCAD 2021.01+
// 
// Các thông số đầu vào thực tế từ người dùng:
// - Kích thước PCB: Rộng 185.2 mm x Dài 175.0 mm
// - Quạt làm mát: 40 x 40 x 20 mm (cắm dây)
// - Màn hình: TFT 2.8 inch (cắm dây)
// - Đèn báo: 3x LED phi 5mm cảnh báo (cắm dây)
// - Công tắc nguồn: 1.8 cm x 1.2 cm (18 x 12 mm, loại bập bênh snap-in, cắm dây)
// ==============================================================================

// ------------------------------------------------------------------------------
// 1. CẤU HÌNH HIỂN THỊ (VIEW CONFIGURATION)
// ------------------------------------------------------------------------------
// Hỗ trợ cả dạng chuỗi và biến định danh
exploded    = "exploded";
assembled   = "assembled";
bottom_only = "bottom_only";
top_only    = "top_only";
cutaway     = "cutaway";

// Lựa chọn chế độ xem:
// "exploded"    : Bản vẽ phân rã (Báo cáo ĐATN & Slide thuyết trình)
// "assembled"   : Sản phẩm hoàn thiện lắp ráp đầy đủ
// "bottom_only" : Chỉ xuất nắp dưới (để in 3D STL)
// "top_only"    : Chỉ xuất nắp trên (để in 3D STL)
// "cutaway"     : Mặt cắt ngang 1/2 để kiểm tra linh kiện bên trong
VIEW_MODE = "exploded"; 

// Khoảng cách kéo tách giữa nắp trên và nắp dưới trong chế độ "exploded" (mm)
EXPLODE_Z = 75; 

// Độ mịn các đường cong tròn ($fn)
$fn = 40;

// ------------------------------------------------------------------------------
// 2. THÔNG SỐ HÌNH HỌC (PARAMETRIC DIMENSIONS)
// ------------------------------------------------------------------------------
// Kích thước bo mạch PCB thực tế từ người dùng
pcb_w = 207.0;       // Chiều rộng PCB thực tế: 20.7 cm (207.0 mm)
pcb_l = 175.0;       // Chiều dài PCB thực tế: 17.5 cm (175.0 mm)
pcb_t = 1.6;         // Độ dày phíp PCB FR4 (trục Z)
enable_standoffs = false; // Bỏ bệ đỡ: Đáy hộp phẳng lì 100%, tự do đặt bo mạch
pcb_standoff_h = enable_standoffs ? 6.0 : 0.0; // Chiều cao trụ đỡ PCB cách đáy hộp

// Kích thước thành vỏ & Khoảng hở dung sai
wall = 2.5;                // Độ dày thành vỏ (khuyến nghị cho in 3D PETG/ABS)
corner_r = 6.0;            // Bán kính bo tròn 4 góc hộp
clearance_x_left = 8.0;    // Khoảng hở mép trái (bên khối pin 18650)
clearance_x_right = 24.0;  // Khoảng hở mép phải (cho quạt 20mm + 4mm hở an toàn không đè mạch)
clearance_y = 12.0;        // Khoảng hở trước/sau bo mạch

// Kích thước phủ bì vỏ hộp (External dimensions)
outer_w = pcb_w + clearance_x_left + clearance_x_right + 2 * wall; // = 244.0 mm (~24.4 cm)
outer_l = pcb_l + 2 * (clearance_y + wall);                         // = 204.0 mm (~20.4 cm)
outer_h = 52.0;                                                     // Tổng chiều cao hộp hoàn thiện (5.2 cm)

lid_t = 3.0;                                // Độ dày tấm NẮP PHẲNG (Flat Cover Plate)
bottom_h = outer_h - lid_t;                 // Chiều cao thân dưới = 49.0 mm (chứa quạt 40mm sát đáy)
top_h = lid_t;                              // Chiều cao nắp = 3.0 mm (tấm phẳng)

// Thông số Quạt tản nhiệt (40 x 40 x 20 mm)
fan_w = 40.0;
fan_h = 40.0;
fan_t = 20.0;
fan_screw_pitch = 32.0; // Khoảng cách tâm 4 lỗ vít quạt 4cm
fan_screw_d = 3.2;      // Đường kính lỗ ốc M3 (chuẩn zin theo quạt 40x40, tương thích cả M2.5 và M3)

// Thông số Màn hình TFT 2.8" (Khung kính 7.0 cm x 5.1 cm - đã nới thêm 1mm theo yêu cầu)
tft_active_w = 71.0;    // Chiều dài X: 71.0mm (đã nới rộng thêm >1mm để thả kính vào êm ru)
tft_active_h = 51.5;    // Chiều rộng Y: 51.5mm (đã nới rộng thêm để thả kính vào êm ru)
tft_pcb_w = 86.0;       // Kích thước cả bo màn hình
tft_pcb_h = 50.0;
tft_pcb_t = 3.5;
tft_screw_pitch_x = 78.0; // Cự ly tâm ốc X chuẩn bo đỏ (78.0mm)
tft_screw_pitch_y = 43.0; // Cự ly tâm ốc Y chuẩn bo đỏ (43.0mm)

// Thông số Công tắc nguồn bập bênh (Chiều rộng 2.0 cm = 20mm theo yêu cầu)
switch_cut_w = 20.2;    // Chiều rộng công tắc: 20.0mm + 0.2mm dung sai = 20.2mm (chuẩn 2.0cm)
switch_cut_h = 13.0;    // Chiều dài công tắc: 12.5 - 13.0mm (chuẩn snap-in bập bênh)

// Thông số 3 LED cảnh báo
led_hole_d = 5.2;       // Lỗ cho LED 5mm
led_pitch = 14.0;       // Khoảng cách giữa các LED

// ------------------------------------------------------------------------------
// 3. MÀU SẮC ĐỒ HỌA MÔ HÌNH (COLOR PALETTE)
// ------------------------------------------------------------------------------
COLOR_BOTTOM = [0.20, 0.22, 0.25]; // Xám đậm nhám (Matte Charcoal)
COLOR_TOP    = [0.15, 0.16, 0.18]; // Đen mờ công nghiệp
COLOR_PCB    = [0.08, 0.40, 0.20]; // Xanh lá đậm chuẩn phíp FR4
COLOR_SCREEN = [0.05, 0.70, 0.90]; // Xanh Cyan hiển thị (như EV UI)
COLOR_FAN    = [0.10, 0.10, 0.10]; // Đen cánh quạt
COLOR_SWITCH = [0.80, 0.15, 0.15]; // Đỏ công tắc
COLOR_GOLD   = [0.85, 0.70, 0.20]; // Vàng đồng (đai ốc đồng)
COLOR_SILVER = [0.75, 0.75, 0.80]; // Inox ốc vít

// ==============================================================================
// CÁC MODULE CƠ BẢN (HELPER MODULES)
// ==============================================================================

// Khối hộp bo tròn 4 góc (rất nhẹ, chạy cực nhanh trong OpenSCAD)
module rounded_box(w, l, h, r) {
    hull() {
        translate([r, r, 0]) cylinder(r=r, h=h);
        translate([w - r, r, 0]) cylinder(r=r, h=h);
        translate([r, l - r, 0]) cylinder(r=r, h=h);
        translate([w - r, l - r, 0]) cylinder(r=r, h=h);
    }
}

// Lưới tổ ong bảo vệ quạt (Hexagonal Mesh)
module hex_grille(radius, hex_r, spacing) {
    intersection() {
        cylinder(r=radius, h=wall + 2, center=true);
        union() {
            for (x = [-radius : spacing : radius]) {
                for (y = [-radius : spacing : radius]) {
                    translate([x + ((floor(y/spacing)%2)*spacing/2), y, 0])
                        rotate([0, 0, 30])
                        cylinder(r=hex_r, h=wall + 4, center=true, $fn=6);
                }
            }
        }
    }
}

// Khe gió tản nhiệt đối lưu (Air Louvers)
module air_slots(count, slot_w, slot_h, pitch) {
    for (i = [0 : count - 1]) {
        translate([0, i * pitch, 0])
            hull() {
                translate([0, 0, 0]) rotate([0, 90, 0]) cylinder(r=slot_h/2, h=wall*3, center=true);
                translate([0, slot_w - slot_h, 0]) rotate([0, 90, 0]) cylinder(r=slot_h/2, h=wall*3, center=true);
            }
    }
}

// ==============================================================================
// 4. MÔ PHỎNG LINH KIỆN NỘI BỘ (VIRTUAL ELECTRONICS)
// ==============================================================================

// Mô hình Bo mạch PCB chính + Linh kiện
module model_pcb() {
    color(COLOR_PCB) {
        // Tấm mạch FR4
        difference() {
            rounded_box(pcb_w, pcb_l, pcb_t, 3.0);
            // 4 lỗ bắt ốc ở 4 góc bo mạch
            translate([5, 5, -1]) cylinder(r=1.7, h=pcb_t+2);
            translate([pcb_w - 5, 5, -1]) cylinder(r=1.7, h=pcb_t+2);
            translate([5, pcb_l - 5, -1]) cylinder(r=1.7, h=pcb_t+2);
            translate([pcb_w - 5, pcb_l - 5, -1]) cylinder(r=1.7, h=pcb_t+2);
        }
    }
    
    // 1. KHỐI 4 PIN 18650 MÀU XANH LÁ Ở NỬA TRÁI BO MẠCH (Như ảnh thật)
    color([0.15, 0.15, 0.15]) // Khay pin đen
        translate([15, 30, pcb_t]) cube([80, 115, 22]);
        
    color([0.45, 0.85, 0.25]) { // 4 Cell pin 18650 xanh lá
        for (i = [0 : 3]) {
            translate([15 + 8, 30 + 15 + i*28, pcb_t + 11])
                rotate([0, 90, 0]) cylinder(r=9.3, h=65);
        }
    }
    
    // 2. Vi điều khiển ESP32 (ở trên) & STM32 Blue Pill (ở dưới)
    color([0.15, 0.15, 0.18]) {
        translate([pcb_w - 75, pcb_l - 60, pcb_t + 2]) cube([52, 28, 4]); // ESP32 board
    }
    color([0.1, 0.35, 0.7]) {
        translate([pcb_w - 75, 55, pcb_t + 2]) cube([53, 23, 4]); // STM32 Blue Pill
    }
    
    // 3. Mạch hạ áp Buck DC-DC (ở giữa)
    color([0.1, 0.3, 0.6])
        translate([pcb_w - 75, 95, pcb_t + 2]) cube([45, 22, 14]);
    color([0.8, 0.45, 0.1]) // Cuộn cảm dây đồng tròn
        translate([pcb_w - 55, 106, pcb_t + 8]) cylinder(r=7, h=10);
        
    // 4. Dàn Diodes & MOSFETs ở góc dưới (ngay trước quạt)
    color([0.2, 0.25, 0.3])
        translate([pcb_w - 65, 20, pcb_t]) cube([45, 15, 10]);
        
    // 5. Còi báo Buzzer
    color([0.1, 0.1, 0.1])
        translate([pcb_w - 85, pcb_l - 25, pcb_t]) cylinder(r=6, h=9);
}

// Mô hình Quạt 40x40x20mm (thổi gió ngang qua thành phải)
module model_fan() {
    color(COLOR_FAN) {
        difference() {
            cube([fan_t, fan_w, fan_h], center=true);
            // Lỗ thổi cánh quạt
            rotate([0, 90, 0]) cylinder(r=18.5, h=fan_t+2, center=true);
            // 4 lỗ vít góc
            for (dy = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
                for (dz = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
                    translate([0, dy, dz]) rotate([0, 90, 0])
                        cylinder(r=fan_screw_d/2, h=fan_t+2, center=true);
                }
            }
        }
        // Trục quạt & cánh quạt
        rotate([0, 90, 0]) cylinder(r=8, h=fan_t*0.9, center=true);
    }

    // 4 Ốc kim loại M3 bắt cố định quạt vào thành vỏ hộp (tương thích M2.5 / M3)
    color(COLOR_SILVER) {
        for (dy = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
            for (dz = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
                translate([fan_t/2 + wall/2, dy, dz]) rotate([0, 90, 0]) {
                    cylinder(r=1.5, h=fan_t + wall + 2, center=true);
                    // Đầu ốc chìm M3 phẳng lì mặt ngoài vỏ hộp
                    translate([0, 0, (fan_t + wall)/2])
                        cylinder(r=3.0, h=1.0, center=true);
                }
            }
        }
    }
}

// Mô hình Màn hình TFT 2.8" cắm dây
module model_tft() {
    // Mạch PCB TFT
    color([0.1, 0.2, 0.6])
        translate([-tft_pcb_w/2, -tft_pcb_h/2, 0])
            cube([tft_pcb_w, tft_pcb_h, tft_pcb_t]);
            
    // Vùng kính hiển thị Active Area
    color(COLOR_SCREEN)
        translate([-tft_active_w/2, -tft_active_h/2, tft_pcb_t])
            cube([tft_active_w, tft_active_h, 1.5]);
}

// Mô hình Công tắc nguồn 20x15mm
module model_switch() {
    color([0.1, 0.1, 0.1]) // Viền nhựa công tắc
        cube([switch_cut_w + 3, switch_cut_h + 3, 4], center=true);
    color(COLOR_SWITCH)    // Phím bập bênh đỏ
        translate([0, 0, 2.5]) cube([switch_cut_w - 2, switch_cut_h - 2, 5], center=true);
}

// ==============================================================================
// 5. THÂN DƯỚI (BOTTOM SHELL)
// ==============================================================================
module bottom_shell() {
    color(COLOR_BOTTOM) {
        difference() {
            // Thân vỏ hộp nguyên khối cao 62.0mm (chứa toàn bộ chiều cao của hộp)
            rounded_box(outer_w, outer_l, bottom_h, corner_r);
            
            // Khoét rỗng ruột thân dưới (độ dày thành wall = 2.5mm)
            translate([wall, wall, wall])
                rounded_box(outer_w - 2*wall, outer_l - 2*wall, bottom_h + 10, corner_r - wall);

            // -------------------------------------------------------------
            // CẮT KHOÉT NGOẠI VI: CỬA SỔ QUẠT CẠNH PHẢI (CHUẨN BẮT 4 ỐC M3 SÍT THÀNH HỘP)
            // -------------------------------------------------------------
            fan_pos_y = outer_l * 0.42;           // ~85.7mm (thẳng hàng con MOSFET TO-220)
            fan_pos_z = 26.0;                     // Tâm quạt 40x40mm ở Z = 26mm (cách sàn đáy 6mm, cách mép trên 3mm)
            
            // 1. Khung viền chỉ vuông 40.4 x 40.4mm âm 0.6mm ở mặt ngoài (tạo thẩm mỹ viền vuông)
            translate([outer_w - 0.3, fan_pos_y, fan_pos_z])
                cube([0.8, 40.4, 40.4], center=true);

            // 2. Lỗ thoát gió tròn Ø38.0mm thông suốt qua thành vỏ (vừa khít 100% vòng cánh quạt)
            translate([outer_w - wall/2, fan_pos_y, fan_pos_z])
                rotate([0, 90, 0])
                    cylinder(r=19.0, h=wall * 3, center=true);

            // 3. 4 Lỗ bắt ốc M3 chuyên dụng (Ø3.4mm) chuẩn cự ly 32 x 32mm + Lỗ loe âm đầu ốc Ø6.4mm
            for (dy = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
                for (dz = [-fan_screw_pitch/2, fan_screw_pitch/2]) {
                    // Lỗ xỏ ốc M3 qua thành (đường kính Ø3.4mm vừa khít êm)
                    translate([outer_w - wall/2, fan_pos_y + dy, fan_pos_z + dz])
                        rotate([0, 90, 0])
                            cylinder(r=1.7, h=wall * 3, center=true);
                    // Lỗ khoét âm cho đầu ốc M3 nằm phẳng lì mặt ngoài vỏ hộp (Flush Countersink Ø6.4mm)
                    translate([outer_w - 0.5, fan_pos_y + dy, fan_pos_z + dz])
                        rotate([0, 90, 0])
                            cylinder(r=3.2, h=1.2, center=true);
                }
            }

            // 4. KHE GIÓ HÚT ĐỐI LƯU Ở CẠNH TRÁI ĐỐI DIỆN (LEFT WALL - X = 0)
            // Không khí mát hút từ cạnh trái, quét qua khối Pin 18650 & STM32,
            // rồi quạt ở cạnh phải thổi gió nóng qua MOSFET ra ngoài
            translate([wall/2, outer_l/2 - 40, 20]) {
                rotate([0, 0, 90])
                    air_slots(count=7, slot_w=30, slot_h=3.2, pitch=8.0);
            }
        }
    }

    // -----------------------------------------------------------------
    // KHUNG ĐỠ & RÃNH ĐỊNH VỊ QUẠT 40x40x20mm Ở MẶT TRONG THÀNH HỘP
    // Giúp thả quạt vào là tự động khớp 100% 4 lỗ ốc, ôm khít sít rịt!
    // -----------------------------------------------------------------
    fan_pos_y = outer_l * 0.42;
    fan_pos_z = 26.0;
    
    // 1. Bệ đỡ đáy quạt (ở độ cao Z = 6.0mm, dày 2mm, chỉ nằm ở mép phải sát quạt)
    translate([outer_w - wall - fan_t/2, fan_pos_y, 5.0]) {
        color(COLOR_BOTTOM)
            cube([fan_t + 1.0, fan_w + 3.0, 2.0], center=true);
    }
    
    // 2. Hai gờ định vị ôm 2 bên sườn quạt (rộng 40.4mm, cao 12mm)
    for (dy = [-fan_w/2 - 1.0, fan_w/2 + 1.0]) {
        translate([outer_w - wall - fan_t/2, fan_pos_y + dy, 12.0]) {
            color(COLOR_BOTTOM)
                cube([fan_t, 2.0, 12.0], center=true);
        }
    }

    // -----------------------------------------------------------------
    // ĐÁY HỘP PHẲNG HOÀN TOÀN (BỎ HẲN BỆ ĐỠ THEO YÊU CẦU NGƯỜI DÙNG)
    // Toàn bộ lòng đáy hộp hoàn toàn phẳng lì 100%, không có trụ, không có khấc.
    // Người dùng thả bo mạch tự do, có thể lót mút xốp đệm chống sốc cách điện tùy ý.
    // -----------------------------------------------------------------
    if (enable_standoffs) {
        pcb_origin_x = wall + clearance_x_left;
        pcb_origin_y = wall + clearance_y;

        // 1. 4 Bệ đỡ phẳng tròn ở 4 góc bo mạch (bán kính R=7.0mm, cao đúng 6.0mm)
        translate([pcb_origin_x + 12, pcb_origin_y + 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
        translate([pcb_origin_x + pcb_w - 12, pcb_origin_y + 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
        translate([pcb_origin_x + 12, pcb_origin_y + pcb_l - 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
        translate([pcb_origin_x + pcb_w - 12, pcb_origin_y + pcb_l - 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);

        // 2. Hai bệ đỡ phẳng ở giữa cạnh Trước và Sau
        translate([pcb_origin_x + pcb_w/2, pcb_origin_y + 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
        translate([pcb_origin_x + pcb_w/2, pcb_origin_y + pcb_l - 12, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);

        // 3. Hai bệ đỡ phẳng chịu tải trọng dưới khối Pin 18650
        translate([pcb_origin_x + 40, pcb_origin_y + 40, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
        translate([pcb_origin_x + 85, pcb_origin_y + 40, wall])
            color(COLOR_BOTTOM) cylinder(r=7.0, h=6.0);
    }
}

// ==============================================================================
// 6. NẮP TRÊN (TOP SHELL)
// ==============================================================================
module top_shell() {
    color(COLOR_TOP) {
        difference() {
            union() {
                // 1. TẤM NẮP PHẲNG (FLAT LID PLATE - DÀY 3.0mm)
                rounded_box(outer_w, outer_l, lid_t, corner_r);
                
                // 2. GỜ VIỀN ĐỊNH VỊ ÔM KHÍT VÀO LÒNG HỘP (INNER RIM COLLAR - CAO 4.5mm)
                // Ma sát tự nhiên giữ nắp phẳng khít rịt, không cần ốc, không cần lẫy nhựa
                translate([wall + 0.25, wall + 0.25, -4.5])
                    rounded_box(outer_w - 2*(wall + 0.25), outer_l - 2*(wall + 0.25), 4.5, corner_r - wall);
            }
            
            // Khoét rỗng giữa gờ định vị để nắp nhẹ và tiết kiệm nhựa
            translate([wall + 2.5, wall + 2.5, -5.0])
                rounded_box(outer_w - 2*(wall + 2.5), outer_l - 2*(wall + 2.5), 5.5, corner_r - wall - 2.0);

            // -------------------------------------------------------------
            // KHOÉT LỖ BỀ MẶT THAO TÁC (TRÊN TẤM NẮP PHẲNG)
            // -------------------------------------------------------------
            
            // 1. Cửa sổ Màn hình TFT 2.8" (có vát viền Bezel 45 độ)
            tft_center_x = 80;
            tft_center_y = outer_l / 2 + 10;
            translate([tft_center_x, tft_center_y, lid_t/2]) {
                cube([tft_active_w, tft_active_h, lid_t + 2], center=true);
                // Vát viền mép trên
                translate([0, 0, lid_t/2])
                    hull() {
                        cube([tft_active_w, tft_active_h, 0.1], center=true);
                        translate([0, 0, 1.2])
                            cube([tft_active_w + 2.5, tft_active_h + 2.5, 0.1], center=true);
                    }
            }

            // 2. 3 Lỗ cho LED Cảnh báo (Ø5.2mm)
            led_center_x = 180;
            led_base_y   = outer_l / 2 + 35;
            for (i = [0 : 2]) {
                translate([led_center_x + (i - 1)*led_pitch, led_base_y, -1]) {
                    cylinder(r=led_hole_d/2, h=lid_t + 2);
                    translate([0, 0, lid_t - 0.5])
                        cylinder(r1=led_hole_d/2, r2=led_hole_d/2 + 1.0, h=1.0);
                }
            }

            // 3. Lỗ Công tắc nguồn bập bênh (20.2 x 13.0 mm)
            switch_pos_x = 180;
            switch_pos_y = outer_l / 2 - 20;
            translate([switch_pos_x, switch_pos_y, lid_t/2]) {
                cube([switch_cut_w, switch_cut_h, lid_t + 2], center=true);
            }
            // Vát mỏng mặt trong lỗ công tắc xuống 1.8mm để ngàm nhựa bập bênh búng khóa chắc
            translate([switch_pos_x, switch_pos_y, -1.0])
                cube([switch_cut_w + 6, switch_cut_h + 6, 2.4], center=true);

            // 4. Lỗ thoát âm cho Còi Buzzer (Cụm 5 lỗ tổ ong nhỏ)
            buzzer_pos_x = 180;
            buzzer_pos_y = outer_l / 2 - 55;
            translate([buzzer_pos_x, buzzer_pos_y, -1]) {
                cylinder(r=1.2, h=lid_t + 2);
                for (a = [0 : 60 : 300]) {
                    rotate([0, 0, a]) translate([3.5, 0, 0])
                        cylinder(r=1.0, h=lid_t + 2);
                }
            }

            // 5. Khe cậy ngón tay ở mép ngoài (Pry Notch) để tháo nắp nhanh bằng tay
            translate([outer_w/2, 0.5, lid_t - 1.5])
                cube([25.0, 3.0, 2.0], center=true);

            // 6. Rãnh né đỉnh quạt 40x40mm ở gờ viền trong (đảm bảo đậy nắp phẳng êm, không cấn quạt)
            translate([outer_w - wall - 1.0, outer_l * 0.42, -2.5])
                cube([wall * 3, fan_w + 4.0, 5.0], center=true);
        }
    }

    // 4 Cụm trụ ốc âm M3 chuyên dụng bắt bo màn hình TFT 2.8" dưới tấm nắp phẳng
    // Thiết kế dạng trụ bầu dục gia cường: dày Ø7.2mm x 10.0mm, rãnh mồi Ø2.5mm chuẩn cho ren M3
    // Ren M3 tự ta-rô bám cực kỳ cứng cáp và chắc nịch, không sợ nứt nhựa!
    tft_center_x = 80;
    tft_center_y = outer_l / 2 + 10;
    for (dir_x = [-1, 1]) {
        for (dir_y = [-1, 1]) {
            bx = tft_center_x + dir_x * (tft_screw_pitch_x/2);
            by = tft_center_y + dir_y * (tft_screw_pitch_y/2);
            translate([bx, by, -4.0]) {
                color(COLOR_TOP) {
                    difference() {
                        // Trụ nhựa hình bầu dục gia cường chắc nịch Ø7.2mm x 10.0mm
                        hull() {
                            translate([0, -1.5, 0]) cylinder(r=3.6, h=4.0);
                            translate([0,  1.5, 0]) cylinder(r=3.6, h=4.0);
                        }
                        // Rãnh mồi ốc M3 liền khối Ø2.5mm (vừa khít cự ly 43mm đến 48mm, không lo lệch lỗ)
                        translate([0, 0, -0.5])
                            hull() {
                                cylinder(r=1.25, h=5.0);
                                translate([0, dir_y * 2.5, 0]) cylinder(r=1.25, h=5.0);
                            }
                    }
                }
            }
        }
    }
}

// ==============================================================================
// 7. TỔNG HỢP LẮP RÁP HỆ THỐNG (SYSTEM ASSEMBLY)
// ==============================================================================
module complete_assembly() {
    // 1. Thân dưới cố định ở mặt sàn Z = 0
    bottom_shell();

    // 2. Bo mạch PCB đặt trên các trụ đỡ
    translate([wall + clearance_x_left, wall + clearance_y, wall + pcb_standoff_h])
        model_pcb();

    // 3. Quạt 40x40x20mm đặt ở CẠNH PHẢI (ngang tầm MOSFET TO-220)
    translate([outer_w - wall - fan_t/2, outer_l * 0.42, 26.0])
        model_fan();

    // 4. Các chi tiết gắn trên Tấm Nắp Phẳng
    explode_shift = (VIEW_MODE == "exploded") ? EXPLODE_Z : 0;
    
    translate([0, 0, bottom_h + explode_shift]) {
        // Tấm nắp phẳng
        top_shell();
        
        // Màn hình TFT 2.8" áp dưới nắp
        translate([80, outer_l / 2 + 10, -tft_pcb_t])
            model_tft();
            
        // 3 LED cảnh báo
        led_center_x = 180;
        led_base_y   = outer_l / 2 + 35;
        // LED 1: Đỏ (Fault)
        color([0.9, 0.1, 0.1])
            translate([led_center_x - led_pitch, led_base_y, 0]) cylinder(r=2.5, h=5);
        // LED 2: Vàng (Warning)
        color([0.9, 0.8, 0.1])
            translate([led_center_x, led_base_y, 0]) cylinder(r=2.5, h=5);
        // LED 3: Xanh (Run / Normal)
        color([0.1, 0.9, 0.2])
            translate([led_center_x + led_pitch, led_base_y, 0]) cylinder(r=2.5, h=5);
            
        // Công tắc nguồn
        translate([180, outer_l / 2 - 20, 0])
            model_switch();
    }
}

// ==============================================================================
// 8. ĐIỀU KHIỂN XUẤT MÔ HÌNH THEO CHẾ ĐỘ CHỌN
// ==============================================================================
if (VIEW_MODE == "bottom_only") {
    // Xuất file STL nắp dưới để in 3D
    bottom_shell();
} 
else if (VIEW_MODE == "top_only") {
    // Lật ngược nắp trên 180 độ để mặt phẳng nằm dưới bàn in 3D
    rotate([180, 0, 0])
        translate([0, -outer_l, -top_h])
            top_shell();
} 
else if (VIEW_MODE == "cutaway") {
    // Cắt ngang 1/2 để quan sát không gian luồn dây & linh kiện
    difference() {
        complete_assembly();
        translate([-10, outer_l/2, -10])
            cube([outer_w + 20, outer_l, outer_h + EXPLODE_Z + 20]);
    }
}
else {
    // Chế độ "exploded" hoặc "assembled"
    complete_assembly();
}
