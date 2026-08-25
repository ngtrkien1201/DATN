import time
import math
import datetime
import random

# ============================================================
# BẢNG TRA CỨU OCV-SOC (Lookup Table) cho Pin Li-ion 18650
# ============================================================
def interpolate_ocv(soc):
    soc_points = [0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100]
    ocv_points = [2.80, 3.20, 3.40, 3.48, 3.52, 3.56, 3.60, 3.63, 3.65, 3.67, 3.70, 3.73, 3.75, 3.78, 3.80, 3.85, 3.90, 3.95, 4.00, 4.10, 4.20]
    if soc <= 0: return ocv_points[0]
    if soc >= 100: return ocv_points[-1]
    for i in range(len(soc_points) - 1):
        if soc_points[i] <= soc <= soc_points[i + 1]:
            x0, x1 = soc_points[i], soc_points[i + 1]
            y0, y1 = ocv_points[i], ocv_points[i + 1]
            return y0 + (y1 - y0) * ((soc - x0) / (x1 - x0))
    return 3.7

class BatteryTwin:
    def __init__(self):
        # 1. THÔNG SỐ VẬT LÝ CỦA MÔ HÌNH MẠCH TƯƠNG ĐƯƠNG (ECM Thevenin 1RC)
        self.capacity_Ah = 2.2 # Mặc định 2.2Ah
        self.capacity_As = self.capacity_Ah * 3600
        self.R0_nominal = 0.045
        self.R0 = self.R0_nominal
        self.R1 = 0.020
        self.C1 = 500.0

        # 2. BIẾN TRẠNG THÁI CỦA MÔ HÌNH (State Variables)
        self.V_p = 0.0
        self.twin_ocv = 0.0
        self.twin_terminal_v = 0.0
        self.internal_twin_soc = None
        self.internal_twin_soh = 100.0
        self.internal_twin_temp = None
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
            status = "Charging" if I > 0.01 else ("Discharging" if I < -0.01 else "Idle")
        self.real_state['status'] = status

        if dt > 5.0 or self.internal_twin_soc is None:
            self.internal_twin_soc = self.real_state['soc']
            self.internal_twin_soh = self.real_state['soh']
            self.internal_twin_temp = self.real_state['temperature']
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
        if abs(I) > 0.05:
            estimated_R0 = abs(self.real_state['voltage'] - self.twin_ocv - self.V_p) / abs(I)
            estimated_R0 = min(max(estimated_R0, 0.01), 0.2)
            self.R0 = self.R0 * 0.95 + estimated_R0 * 0.05
            
        self.residual = round(abs(self.V_p - prev_Vp), 6)
        self.convergence = "OK" if self.residual < 0.01 else "Converging"

        self.twin_terminal_v = self.twin_ocv + I * self.R0 + self.V_p

        # BƯỚC 3: Cập nhật SOC, Dung lượng, Chu kỳ, Nhiệt độ
        self.internal_twin_soc += (I * dt) / self.capacity_As * 100.0
        self.internal_twin_soc = max(0.0, min(100.0, self.internal_twin_soc))
        self.remaining_capacity = round(self.capacity_Ah * (self.internal_twin_soc / 100.0), 3)

        self.coulomb_accumulator += abs(I) * dt
        if self.coulomb_accumulator >= self.capacity_As:
            self.cycle_count += 1
            self.coulomb_accumulator -= self.capacity_As

        soh_from_resistance = max(0, min(100, 100 * (1 - (self.R0 - self.R0_nominal) / self.R0_nominal)))
        self.internal_twin_soh = self.internal_twin_soh * 0.99 + soh_from_resistance * 0.01

        alpha = 0.05
        self.internal_twin_temp = self.internal_twin_temp * (1 - alpha) + self.real_state['temperature'] * alpha

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
            f_soc = max(0.0, min(100.0, self.internal_twin_soc + (I_avg * dt_s) / self.capacity_As * 100.0))
            f_ocv = interpolate_ocv(f_soc)
            tau = self.R1 * self.C1
            f_Vp = self.V_p * math.exp(-dt_s / tau) + I_avg * self.R1 * (1 - math.exp(-dt_s / tau))
            f_v = f_ocv + I_avg * self.R0 + f_Vp
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
        
        sim_soc = sim_soc + (I * dt_s) / self.capacity_As * 100.0
        sim_soc = max(0.0, min(100.0, sim_soc))
        sim_ocv = interpolate_ocv(sim_soc)
        tau = self.R1 * self.C1
        sim_Vp = sim_Vp * math.exp(-dt_s / tau) + I * self.R1 * (1 - math.exp(-dt_s / tau))
        sim_v = sim_ocv + I * self.R0 + sim_Vp
        sim_temp = self.internal_twin_temp + (I * I * self.R0 * duration_minutes * 0.1) # Nhiệt tỏa I^2*R
        
        return {
            'target_current': I,
            'duration_mins': duration_minutes,
            'sim_voltage': round(sim_v, 3),
            'sim_soc': round(sim_soc, 2),
            'sim_temp': round(sim_temp, 2),
            'sim_ocv': round(sim_ocv, 3),
            'sim_vp': round(sim_Vp, 4)
        }

    # ================================================================
    # SERIALIZATION FOR SERVERLESS PERSISTENCE
    # ================================================================
    def to_dict(self):
        return {
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
            'last_sync_timestamp': self.last_sync_timestamp
        }

    def from_dict(self, data):
        if not data: return
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
        self.last_sync_timestamp = data.get('last_sync_timestamp', time.time())

    # ================================================================
    # API RESPONSE: Trả về toàn bộ trạng thái cho Dashboard
    # ================================================================
    def get_state(self):
        if time.time() - self.last_sync_timestamp > 10:
            self.sync_status = "Disconnected"
            self.sync_rate = 0.0

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
            4: "Over-heat"
        }
        
        class_name = AI_CLASS_MAP.get(ai_class_idx, f"Unknown Class {ai_class_idx}")
        
        # Nếu Normal, Anomaly score thấp. Nếu Lỗi, Anomaly score cao
        if ai_class_idx == 0:
            anomaly_score = round(1.0 - (ai_confidence / 100.0), 3)
            ai_status_str = "Normal"
        else:
            anomaly_score = round(ai_confidence / 100.0, 3)
            ai_status_str = "Warning" if anomaly_score < 0.8 else "Alert"
            
        # Nếu chưa nhận được dữ liệu AI bao giờ thì báo Waiting
        has_ai_data = self.edge_ai_state.get('AI_Time', 0) > 0
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
            }
        }

battery_twin_instance = BatteryTwin()
