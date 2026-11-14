import time
import math
import datetime
import random

# ============================================================
# BẢNG TRA CỨU OCV-SOC (Lookup Table) cho Pin Li-ion 18650 4S
# Đồng bộ chuẩn với bảng tra trong STM32 (SOC.c)
# ============================================================
def interpolate_ocv(soc, s_count=4):
    soc_points = [0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100]
    ocv_points = [3.30, 3.50, 3.60, 3.68, 3.70, 3.72, 3.74, 3.76, 3.78, 3.80, 3.82, 3.84, 3.87, 3.91, 3.95, 3.98, 4.02, 4.08, 4.11, 4.15, 4.20]
    if soc <= 0: return ocv_points[0] * s_count
    if soc >= 100: return ocv_points[-1] * s_count
    for i in range(len(soc_points) - 1):
        if soc_points[i] <= soc <= soc_points[i + 1]:
            x0, x1 = soc_points[i], soc_points[i + 1]
            y0, y1 = ocv_points[i], ocv_points[i + 1]
            return (y0 + (y1 - y0) * ((soc - x0) / (x1 - x0))) * s_count
    return 3.7 * s_count

class BatteryTwin:
    def __init__(self):
        # 1. THÔNG SỐ VẬT LÝ CỦA MÔ HÌNH MẠCH TƯƠNG ĐƯƠNG (ECM Thevenin 1RC)
        self.capacity_Ah = 2.2 # Mặc định 2.2Ah
        self.capacity_As = self.capacity_Ah * 3600
        self.R0_nominal = 0.045 * 4
        self.R0 = self.R0_nominal
        self.R1 = 0.020 * 4
        self.C1 = 500.0 / 4

        # 2. BIẾN TRẠNG THÁI CỦA MÔ HÌNH (State Variables)
        self.V_p = 0.0
        self.twin_ocv = 0.0
        self.twin_terminal_v = 0.0
        self.internal_twin_soc = None
        self.internal_twin_soh = 100.0
        self.internal_twin_temp = 25.0
        self.remaining_capacity = self.capacity_Ah
        self.cycle_count = 0
        self.coulomb_accumulator = 0.0

        # 3. CHẨN ĐOÁN & ĐÁNH GIÁ (Diagnostics & Confidence)
        self.model_iteration = 0
        self.residual = 0.0
        self.convergence = "OK"
        self.ecm_accuracy = 100.0
        self.pred_confidence = 100.0

        # 4. LỊCH SỬ XU HƯỚNG
        self.resistance_history = []
        self.capacity_history = []
        self.soh_history = []

        # 5. TRẠNG THÁI THỰC TẾ & MÔ PHỎNG
        self.real_state = {'voltage': 0.0, 'current': 0.0, 'temperature': 0.0, 'soc': 0.0, 'soh': 0.0, 'power': 0.0, 'energy': 0.0, 'status': 'Idle'}
        self.twin_state = {'voltage': 0.0, 'current': 0.0, 'temperature': 0.0, 'soc': 0.0, 'soh': 0.0, 'ocv': 0.0, 'terminal_voltage': 0.0, 'polarization_voltage': 0.0, 'internal_resistance': 0.0, 'remaining_capacity': 0.0, 'cycle_count': 0, 'status': 'Idle'}
        self.errors = {'voltage': 0.0, 'voltage_mv': 0.0, 'soc': 0.0, 'soh': 0.0, 'temperature': 0.0}
        self.validation = {'v_mae': 0.0, 'v_rmse': 0.0, 'v_max_err': 0.0, 'soc_mae': 0.0}
        self.error_history = {'v': [], 'soc': []}
        self.edge_ai_state = {'AI_Class': 0, 'AI_Score': 100.0, 'AI_Time': 0}
        
        self.cv = [0.0, 0.0, 0.0, 0.0]
        self.csoc = [0, 0, 0, 0]
        self.csoh = [100, 100, 100, 100]
        self.cai_class = [0, 0, 0, 0]

        # 6. CELL-LEVEL DIGITAL TWIN (TỪNG CELL 18650 ĐỘC LẬP)
        self.cell_R0_nominal = 0.045  # 45 mOhm / cell định mức
        self.cell_R1 = 0.020          # 20 mOhm / cell
        self.cell_C1 = 500.0          # 500 F / cell
        self.cell_twins = [
            {
                'id': i + 1,
                'v_meas': 0.0,
                'ocv': 0.0,
                'v_term': 0.0,
                'v_p': 0.0,
                'v_p_mv': 0.0,
                'r0': self.cell_R0_nominal,
                'r0_mohm': round(self.cell_R0_nominal * 1000, 1),
                'soc': 0.0,
                'soh': 100.0,
                'residual_mv': 0.0,
                'status': 'Normal'
            }
            for i in range(4)
        ]

        self.last_sync_timestamp = time.time()
        self.sync_rate = 0.0
        self.sync_status = "Disconnected"
        self.sync_latency_ms = 0.0

    def sync(self, real_data):
        """DIGITAL TWIN ENGINE - HÀM ĐỒNG BỘ & MÔ PHỎNG CHÍNH"""
        now = time.time()
        dt = now - self.last_sync_timestamp
        self.sync_latency_ms = round(dt * 1000, 1)

        self.real_state['voltage'] = real_data.get('V', 0.0)
        self.real_state['current'] = real_data.get('I', 0.0)
        self.real_state['temperature'] = real_data.get('T', 0.0)
        self.real_state['soc'] = real_data.get('SOC', 0.0)
        self.real_state['soh'] = real_data.get('SOH', 0.0)
        
        # Lưu trữ trạng thái AI (Nếu có gửi lên từ ESP32)
        if 'AI_Class' in real_data:
            self.edge_ai_state['AI_Class'] = real_data['AI_Class']
            self.edge_ai_state['AI_Score'] = real_data.get('AI_Score', 0.0)
            self.edge_ai_state['AI_Time'] = real_data.get('AI_Time', 0)
            
        self.cv = real_data.get('CV', [0.0, 0.0, 0.0, 0.0])
        self.csoc = real_data.get('CSOC', [0, 0, 0, 0])
        self.csoh = real_data.get('CSOH', [100, 100, 100, 100])
        self.cai_class = real_data.get('CAI_Class', [0, 0, 0, 0])
        
        # Ưu tiên lấy Power và Energy từ mạch gửi lên (JSON), nếu không có mới tự tính
        p_json = real_data.get('P')
        if p_json is not None:
            self.real_state['power'] = p_json
        else:
            self.real_state['power'] = round(self.real_state['voltage'] * self.real_state['current'], 3)
            
        e_json = real_data.get('E')
        if e_json is not None:
            self.real_state['energy'] = e_json
        else:
            self.real_state['energy'] = round(self.real_state.get('energy', 0.0) + abs(self.real_state['power']) * (dt / 3600), 4)

        status = real_data.get('Status', '')
        if not status:
            I = self.real_state['current']
            status = "Discharging" if I > 0.01 else ("Charging" if I < -0.01 else "Idle")
        self.real_state['status'] = status

        if dt > 5.0 or self.internal_twin_soc is None:
            self.internal_twin_soc = self.real_state['soc']
            self.internal_twin_soh = self.real_state['soh']
            self.internal_twin_temp = self.real_state['temperature'] if self.real_state.get('temperature') is not None else 25.0
            self.V_p = 0.0
            dt = 1.0

        I = self.real_state['current']

        # BƯỚC 1: Giải phương trình ECM Thevenin
        self.model_iteration += 1
        tau = self.R1 * self.C1
        exp_factor = math.exp(-dt / tau)
        self.twin_ocv = interpolate_ocv(self.internal_twin_soc)
        
        prev_Vp = self.V_p
        self.V_p = self.V_p * exp_factor + I * self.R1 * (1 - exp_factor)
        
        # BƯỚC 2: Ước lượng Điện trở trong (R0) & Convergence
        # V_terminal = OCV - I * R0 - V_p => I * R0 = OCV - V_real - V_p
        if abs(I) > 0.05:
            estimated_R0 = abs(self.twin_ocv - self.real_state['voltage'] - self.V_p) / abs(I)
            estimated_R0 = min(max(estimated_R0, 0.01), 0.2)
            self.R0 = self.R0 * 0.95 + estimated_R0 * 0.05
            
        self.residual = round(abs(self.V_p - prev_Vp), 6)
        self.convergence = "OK" if self.residual < 0.01 else "Converging"

        # Điện áp đầu cực Thevenin 1RC (Xả I > 0: điện áp sụt dưới OCV; Sạc I < 0: điện áp cao hơn OCV)
        self.twin_terminal_v = self.twin_ocv - I * self.R0 - self.V_p

        # BƯỚC 3: Cập nhật SOC, Dung lượng, Chu kỳ, Nhiệt độ
        # Khi xả (I > 0), SOC phải giảm (-)
        self.internal_twin_soc -= (I * dt) / self.capacity_As * 100.0
        self.internal_twin_soc = max(0.0, min(100.0, self.internal_twin_soc))
        self.remaining_capacity = round(self.capacity_Ah * (self.internal_twin_soc / 100.0), 3)

        self.coulomb_accumulator += abs(I) * dt
        if self.coulomb_accumulator >= self.capacity_As:
            self.cycle_count += 1
            self.coulomb_accumulator -= self.capacity_As

        soh_from_resistance = max(0, min(100, 100 * (1 - (self.R0 - self.R0_nominal) / self.R0_nominal)))
        self.internal_twin_soh = self.internal_twin_soh * 0.99 + soh_from_resistance * 0.01

        alpha = 0.05
        meas_temp = self.real_state['temperature'] if self.real_state.get('temperature') is not None else 25.0
        if self.internal_twin_temp is None:
            self.internal_twin_temp = meas_temp
        else:
            self.internal_twin_temp = self.internal_twin_temp * (1 - alpha) + meas_temp * alpha

        # BƯỚC 4: Đồng bộ trạng thái Twin State
        self.twin_state['voltage'] = round(self.twin_terminal_v, 3)
        self.twin_state['current'] = self.real_state['current']
        self.twin_state['temperature'] = round(self.internal_twin_temp, 2)
        self.twin_state['soc'] = round(self.internal_twin_soc, 2)
        self.twin_state['soh'] = round(self.internal_twin_soh, 1)
        self.twin_state['ocv'] = round(self.twin_ocv, 3)
        self.twin_state['terminal_voltage'] = round(self.twin_terminal_v, 3)
        self.twin_state['polarization_voltage'] = round(self.V_p, 4)
        self.twin_state['internal_resistance'] = round(self.R0 * 1000, 2)
        self.twin_state['remaining_capacity'] = self.remaining_capacity
        self.twin_state['cycle_count'] = self.cycle_count
        self.twin_state['status'] = self.real_state['status']

        # BƯỚC 4b: CẬP NHẬT MÔ HÌNH DIGITAL TWIN CHO TỪNG CELL 18650 (CELL-LEVEL TWIN)
        tau_cell = self.cell_R1 * self.cell_C1
        exp_cell = math.exp(-dt / tau_cell)
        
        for idx in range(4):
            c_meas_v = float(self.cv[idx]) if idx < len(self.cv) else 0.0
            if idx < len(self.csoc) and float(self.csoc[idx]) > 0:
                c_soc = float(self.csoc[idx])
            else:
                c_soc = self.internal_twin_soc if self.internal_twin_soc is not None else 50.0
            
            c_ocv = interpolate_ocv(c_soc, s_count=1)
            
            # Cập nhật phân cực Vp cho từng cell
            prev_vp_c = self.cell_twins[idx].get('v_p', 0.0)
            new_vp_c = prev_vp_c * exp_cell + I * self.cell_R1 * (1 - exp_cell)
            self.cell_twins[idx]['v_p'] = new_vp_c
            self.cell_twins[idx]['v_p_mv'] = round(new_vp_c * 1000, 1)
            self.cell_twins[idx]['ocv'] = round(c_ocv, 3)
            self.cell_twins[idx]['v_meas'] = round(c_meas_v, 3)
            self.cell_twins[idx]['soc'] = round(c_soc, 1)
            
            # Ước lượng nội trở R0 từng cell (OCV - V_meas - V_p = I * R0)
            r0_c = self.cell_twins[idx].get('r0', self.cell_R0_nominal)
            if abs(I) > 0.05 and c_meas_v > 1.5:
                est_r0_c = abs(c_ocv - c_meas_v - new_vp_c) / abs(I)
                est_r0_c = min(max(est_r0_c, 0.015), 0.200) # Giới hạn dải hợp lý: 15mOhm - 200mOhm
                r0_c = r0_c * 0.95 + est_r0_c * 0.05
            self.cell_twins[idx]['r0'] = r0_c
            self.cell_twins[idx]['r0_mohm'] = round(r0_c * 1000, 1)
            
            # Điện áp đầu cực mô hình từng cell
            v_term_c = c_ocv - I * r0_c - new_vp_c
            self.cell_twins[idx]['v_term'] = round(v_term_c, 3)
            
            # Dư số sai lệch từng cell
            res_c = abs(c_meas_v - v_term_c)
            self.cell_twins[idx]['residual_mv'] = round(res_c * 1000, 1)
            
            # SOH từng cell
            soh_c_res = max(0, min(100, 100 * (1 - (r0_c - self.cell_R0_nominal) / self.cell_R0_nominal)))
            prev_soh_c = self.cell_twins[idx].get('soh', 100.0)
            self.cell_twins[idx]['soh'] = round(prev_soh_c * 0.99 + soh_c_res * 0.01, 1)
            
            # Đánh giá trạng thái từng cell
            if self.cell_twins[idx]['r0_mohm'] > 70.0 or res_c > 0.08:
                self.cell_twins[idx]['status'] = 'Degraded'
            elif self.cell_twins[idx]['r0_mohm'] > 55.0 or res_c > 0.04:
                self.cell_twins[idx]['status'] = 'Warning'
            else:
                self.cell_twins[idx]['status'] = 'Normal'

        # BƯỚC 5: Tính Sai số & Model Confidence
        v_diff = abs(self.twin_state['voltage'] - self.real_state['voltage'])
        self.errors['voltage_mv'] = round(v_diff * 1000, 1)
        if self.real_state['voltage'] > 0:
            self.errors['voltage'] = round(v_diff / self.real_state['voltage'] * 100, 2)
            
        soc_diff = abs(self.twin_state['soc'] - self.real_state['soc'])
        if self.real_state['soc'] > 0:
            self.errors['soc'] = round(soc_diff / self.real_state['soc'] * 100, 2)
            
        # Tính MAE, RMSE cho Validation
        self.error_history['v'].append(v_diff)
        self.error_history['soc'].append(soc_diff)
        if len(self.error_history['v']) > 100:
            self.error_history['v'].pop(0)
            self.error_history['soc'].pop(0)
            
        v_hist = self.error_history['v']
        soc_hist = self.error_history['soc']
        self.validation['v_mae'] = round(sum(v_hist) / len(v_hist), 3) if v_hist else 0.0
        self.validation['v_rmse'] = round(math.sqrt(sum(e**2 for e in v_hist) / len(v_hist)), 3) if v_hist else 0.0
        self.validation['v_max_err'] = round(max(v_hist), 3) if v_hist else 0.0
        self.validation['soc_mae'] = round(sum(soc_hist) / len(soc_hist), 2) if soc_hist else 0.0
            
        self.ecm_accuracy = round(max(0.0, 100.0 - self.errors['voltage'] * 5), 1)
        self.pred_confidence = round(max(0.0, 100.0 - (self.errors['soc'] * 2 + self.errors['voltage'] * 3)), 1)

        # Lưu Lịch sử
        self.resistance_history.append(self.twin_state['internal_resistance'])
        self.capacity_history.append(self.remaining_capacity)
        self.soh_history.append(self.twin_state['soh'])
        if len(self.resistance_history) > 100:
            self.resistance_history.pop(0)
            self.capacity_history.pop(0)
            self.soh_history.pop(0)

        # Đánh giá đồng bộ
        self.last_sync_timestamp = now
        if dt <= 1.5:
            self.sync_rate = 99.8
            self.sync_status = "Synchronized"
        elif dt <= 3.0:
            self.sync_rate = 85.5
            self.sync_status = "Syncing"
        else:
            self.sync_rate = round(max(0, 100 - (dt * 5)), 1)
            self.sync_status = "Connection Weak"

    # ================================================================
    # AGING MODEL & RUL CALCULATION
    # ================================================================
    def calculate_aging_metrics(self):
        cap_fade = round(100.0 - self.twin_state['soh'], 1)
        res_growth = round(((self.R0 - self.R0_nominal) / self.R0_nominal) * 100, 1)
        stage = "Normal"
        if cap_fade > 10 or res_growth > 20: stage = "Degraded"
        if cap_fade > 20 or res_growth > 50: stage = "End of Life"
        
        # Công thức tính RUL đơn giản: Giả sử pin chết ở SOH 80% (Fade 20%)
        # Tuổi thọ định mức 1000 chu kỳ
        total_cycles_expected = 1000
        cycles_left = max(0, int(total_cycles_expected * (self.twin_state['soh'] - 80) / 20)) if self.twin_state['soh'] > 80 else 0
        months_left = round(cycles_left / 30, 1) # Giả sử mỗi ngày sạc 1 lần
        
        return {
            'capacity_fade': f"{cap_fade} %",
            'resistance_growth': f"{res_growth} %",
            'aging_stage': stage,
            'rul_cycles': f"{cycles_left} cycles",
            'rul_months': f"{months_left} months",
            'rul_confidence_range': f"{max(0, cycles_left - 80)} - {cycles_left + 120} cycles"
        }

    # ================================================================
    # MULTI-HORIZON PREDICTION
    # ================================================================
    def predict_multi_horizon(self):
        if self.internal_twin_soc is None: return []
        I_avg = self.real_state['current']
        horizons = [30, 60, 360, 1440] # 30m, 1h, 6h, 24h
        preds = []
        for m in horizons:
            dt_s = m * 60
            f_soc = max(0.0, min(100.0, self.internal_twin_soc - (I_avg * dt_s) / self.capacity_As * 100.0))
            f_ocv = interpolate_ocv(f_soc)
            tau = self.R1 * self.C1
            f_Vp = self.V_p * math.exp(-dt_s / tau) + I_avg * self.R1 * (1 - math.exp(-dt_s / tau))
            f_v = f_ocv - I_avg * self.R0 - f_Vp
            lbl = f"{m}m" if m < 60 else f"{m//60}h"
            preds.append({
                'horizon': lbl,
                'voltage': round(f_v, 2),
                'soc': round(f_soc, 1),
                'capacity': round(self.capacity_Ah * (f_soc / 100.0), 2)
            })
        return preds

    # ================================================================
    # "WHAT IF" SCENARIO SIMULATION SANDBOX
    # ================================================================
    def simulate_what_if(self, target_current, duration_minutes):
        """Tạo Sandbox để giả lập mà không thay đổi biến trạng thái thật"""
        if self.internal_twin_soc is None:
            return {"error": "Twin Engine chưa khởi động"}
            
        sim_soc = self.internal_twin_soc
        sim_Vp = self.V_p
        dt_s = duration_minutes * 60
        I = float(target_current)
        
        sim_soc = sim_soc - (I * dt_s) / self.capacity_As * 100.0
        sim_soc = max(0.0, min(100.0, sim_soc))
        sim_ocv = interpolate_ocv(sim_soc)
        tau = self.R1 * self.C1
        sim_Vp = sim_Vp * math.exp(-dt_s / tau) + I * self.R1 * (1 - math.exp(-dt_s / tau))
        sim_v = sim_ocv - I * self.R0 - sim_Vp
        base_temp = self.internal_twin_temp if self.internal_twin_temp is not None else 25.0
        sim_temp = base_temp + (I * I * self.R0 * duration_minutes * 0.1) # Nhiệt tỏa I^2*R
        
        # Đánh giá mức độ an toàn của kịch bản & Tính Giới hạn dòng an toàn (SOP)
        is_safe = True
        risk_reasons = []
        if sim_v < 11.2:
            is_safe = False
            risk_reasons.append("Sụt áp quá ngưỡng cắt (V < 11.2V)")
        if sim_temp > 45.0:
            is_safe = False
            risk_reasons.append(f"Quá nhiệt ({round(sim_temp, 1)}°C > 45°C)")
        if sim_soc < 10.0:
            is_safe = False
            risk_reasons.append("Cạn kiệt dung lượng (< 10%)")
            
        max_safe_current = max(0.5, round((sim_ocv - 12.0 - sim_Vp) / max(self.R0, 0.05), 1))
        max_safe_current = min(max_safe_current, 3.5)
        status_eval = "AN TOÀN" if is_safe else "CẢNH BÁO NGUY HIỂM"
        recommendation = "Kịch bản khả thi. Tải vận hành an toàn." if is_safe else f"Khuyến nghị giảm tải. Lý do: {', '.join(risk_reasons)}."
        
        return {
            'target_current': I,
            'duration_mins': duration_minutes,
            'sim_voltage': round(sim_v, 3),
            'sim_soc': round(sim_soc, 2),
            'sim_temp': round(sim_temp, 2),
            'sim_ocv': round(sim_ocv, 3),
            'sim_vp': round(sim_Vp, 4),
            'is_safe': is_safe,
            'status_eval': status_eval,
            'recommendation': recommendation,
            'max_safe_current': max_safe_current
        }

    # ================================================================
    # SERIALIZATION FOR SERVERLESS PERSISTENCE
    # ================================================================
    def to_dict(self):
        return {
            'capacity_Ah': self.capacity_Ah,
            'capacity_As': self.capacity_As,
            'R0': self.R0,
            'V_p': self.V_p,
            'internal_twin_soc': self.internal_twin_soc,
            'internal_twin_soh': self.internal_twin_soh,
            'internal_twin_temp': self.internal_twin_temp,
            'cycle_count': self.cycle_count,
            'coulomb_accumulator': self.coulomb_accumulator,
            'model_iteration': self.model_iteration,
            'real_state': self.real_state,
            'twin_state': self.twin_state,
            'errors': self.errors,
            'validation': self.validation,
            'error_history': self.error_history,
            'resistance_history': self.resistance_history[-50:],
            'capacity_history': self.capacity_history[-50:],
            'soh_history': self.soh_history[-50:],
            'last_sync_timestamp': self.last_sync_timestamp,
            'sync_status': self.sync_status,
            'sync_rate': self.sync_rate,
            'sync_latency_ms': self.sync_latency_ms,
            'cv': self.cv,
            'csoc': self.csoc,
            'csoh': self.csoh,
            'cai_class': self.cai_class,
            'edge_ai_state': self.edge_ai_state,
            'cell_twins': self.cell_twins
        }

    def from_dict(self, data):
        if not data: return
        self.capacity_Ah = data.get('capacity_Ah', self.capacity_Ah)
        self.capacity_As = data.get('capacity_As', self.capacity_Ah * 3600)
        self.R0 = data.get('R0', self.R0)
        self.V_p = data.get('V_p', self.V_p)
        self.internal_twin_soc = data.get('internal_twin_soc', self.internal_twin_soc)
        self.internal_twin_soh = data.get('internal_twin_soh', self.internal_twin_soh)
        self.internal_twin_temp = data.get('internal_twin_temp', self.internal_twin_temp)
        self.cycle_count = data.get('cycle_count', self.cycle_count)
        self.coulomb_accumulator = data.get('coulomb_accumulator', self.coulomb_accumulator)
        self.model_iteration = data.get('model_iteration', self.model_iteration)
        self.real_state = data.get('real_state', self.real_state)
        self.twin_state = data.get('twin_state', self.twin_state)
        self.errors = data.get('errors', self.errors)
        self.validation = data.get('validation', self.validation)
        self.error_history = data.get('error_history', self.error_history)
        self.resistance_history = data.get('resistance_history', self.resistance_history)
        self.capacity_history = data.get('capacity_history', self.capacity_history)
        self.soh_history = data.get('soh_history', self.soh_history)
        self.last_sync_timestamp = data.get('last_sync_timestamp', time.time())
        self.sync_status = data.get('sync_status', 'Disconnected')
        self.sync_rate = data.get('sync_rate', 0.0)
        self.sync_latency_ms = data.get('sync_latency_ms', 0.0)
        self.cv = data.get('cv', self.cv)
        self.csoc = data.get('csoc', self.csoc)
        self.csoh = data.get('csoh', self.csoh)
        self.cai_class = data.get('cai_class', self.cai_class)
        self.edge_ai_state = data.get('edge_ai_state', self.edge_ai_state)
        self.cell_twins = data.get('cell_twins', self.cell_twins)

    # ================================================================
    # API RESPONSE: Trả về toàn bộ trạng thái cho Dashboard
    # ================================================================
    def get_state(self):
        time_since_sync = time.time() - self.last_sync_timestamp
        self.sync_latency_ms = round(time_since_sync * 1000, 1)

        if time_since_sync > 10.0:
            self.sync_status = "Disconnected"
            self.sync_rate = 0.0
        elif time_since_sync <= 2.5:
            self.sync_status = "Synchronized"
            self.sync_rate = 99.8
        elif time_since_sync <= 5.0:
            self.sync_status = "Syncing"
            self.sync_rate = 85.0
        else:
            self.sync_status = "Connection Weak"
            self.sync_rate = 60.0

        dt_str = datetime.datetime.fromtimestamp(self.last_sync_timestamp).strftime('%H:%M:%S')
        aging_data = self.calculate_aging_metrics()
        preds = self.predict_multi_horizon()
        
        # Xử lý Edge AI Data
        ai_class_idx = self.edge_ai_state.get('AI_Class', 0)
        ai_confidence = self.edge_ai_state.get('AI_Score', 100.0)
        ai_time_ms = self.edge_ai_state.get('AI_Time', 0)
        
        AI_CLASS_MAP = {
            0: "Normal Operation",
            1: "Over-voltage",
            2: "Under-voltage",
            3: "Over-current",
            4: "Over-heat",
            5: "Cell Imbalance"
        }
        
        class_name = AI_CLASS_MAP.get(ai_class_idx, f"Unknown Class {ai_class_idx}")
        
        # Phân cấp mức độ nguy hiểm:
        # - Bình thường (Class 0): Normal (Xanh lá)
        # - Lệch cell (Class 5): Mức CẢNH BÁO (Warning / LED Vàng, MOSFET vẫn bật, score ~0.55)
        # - Quá áp (1), Sụt áp (2), Quá dòng (3), Quá nhiệt (4): Mức NGUY HIỂM (Alert / LED Đỏ, ngắt MOSFET, score ~0.99)
        if ai_class_idx == 0:
            anomaly_score = round(1.0 - (ai_confidence / 100.0), 3)
            ai_status_str = "Normal"
        elif ai_class_idx == 5:
            # Lệch cell là CẢNH BÁO (Warning), giữ score trong dải 0.45 - 0.60
            anomaly_score = round(0.40 + (ai_confidence / 100.0) * 0.15, 2)
            ai_status_str = "Warning"
        else:
            # Sự cố vật lý nghiêm trọng: Quá áp, Sụt áp, Quá dòng, Quá nhiệt -> NGUY HIỂM (Alert)
            anomaly_score = round(ai_confidence / 100.0, 3)
            ai_status_str = "Alert"
            
        # Nếu chưa nhận được dữ liệu AI bao giờ, hoặc đã mất kết nối thì báo Waiting
        has_ai_data = (self.edge_ai_state.get('AI_Time', 0) > 0
                       and self.sync_status != "Disconnected")
        ai_run_status = "Running (ESP32-S3)" if has_ai_data else "Waiting for Data"

        return {
            'real': self.real_state,
            'twin': self.twin_state,
            'errors': self.errors,
            'predictions': preds,
            'aging_model': aging_data,
            'sub_modules': {
                'ecm_solver': "Running",
                'soc_estimator': "Running",
                'soh_estimator': "Running",
                'prediction_engine': "Running",
                'ai_layer': "Running" if has_ai_data else "Standby"
            },
            'diagnostics': {
                'update_freq': "1 Hz",
                'model_iteration': self.model_iteration,
                'residual': self.residual,
                'convergence': self.convergence,
                'ecm_accuracy': f"{self.ecm_accuracy}%",
                'prediction_confidence': f"{self.pred_confidence}%"
            },
            'sync_metrics': {
                'status': self.sync_status,
                'rate': self.sync_rate,
                'last_update': dt_str,
                'latency_ms': self.sync_latency_ms
            },
            'model_parameters': {
                'capacity_Ah': self.capacity_Ah,
                'capacity_As': self.capacity_As,
                'R0': round(self.R0, 4),
                'R0_mOhm': round(self.R0 * 1000, 1),
                'R1': self.R1,
                'C1': self.C1,
                'eta': 0.98
            },
            'validation': self.validation,
            'edge_ai': {
                'status': ai_run_status,
                'status_label': ai_status_str,
                'anomaly_class': class_name,
                'anomaly_score': anomaly_score if has_ai_data else 0.0,
                'confidence': round(ai_confidence, 1) if has_ai_data else 0.0,
                'inference_time': ai_time_ms if has_ai_data else 0
            },
            'metadata': {
                'twin_id': 'Battery-001'
            },
            'cells': {
                'cv': self.cv,
                'csoc': self.csoc,
                'csoh': self.csoh,
                'cai_class': self.cai_class
            },
            'cell_twins': self.cell_twins
        }

battery_twin_instance = BatteryTwin()
