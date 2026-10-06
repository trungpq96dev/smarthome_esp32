# bom_nuoc — firmware ESP32 điều khiển bơm nước

Bản giai đoạn 1: WiFi + trang web nội bộ + toàn bộ logic bơm và bảo vệ.
Chạy được **chỉ với mỗi bo ESP32 cắm USB**, chưa cần cảm biến nào.

---

## 1. Cài phần mềm

Tải **Arduino IDE 2.x** tại arduino.cc/en/software rồi cài.

Mở `File → Preferences`, dán dòng sau vào ô **Additional boards manager URLs**:

```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

Vào `Tools → Board → Boards Manager`, gõ `esp32`, cài gói **esp32 by Espressif Systems**.

**Không cần cài thư viện nào khác.**

## 2. Sửa cấu hình

Mở `bom_nuoc.ino` bằng Arduino IDE (3 file sẽ hiện thành 3 tab).
Sang tab `config.h`, sửa đúng 2 dòng:

```c
#define WIFI_SSID        "TEN_WIFI"
#define WIFI_PASS        "MAT_KHAU"
```

Các thông số còn lại để nguyên, chỉnh sau khi lắp cảm biến thật.

## 3. Nạp

1. Cắm ESP32 bằng **cáp USB có truyền dữ liệu** (cáp chỉ sạc sẽ không nhận).
2. `Tools → Board → esp32 → ESP32 Dev Module`.
3. `Tools → Partition Scheme` chọn **Minimal SPIFFS (1.9MB APP with OTA / 190KB SPIFFS)**.
   Mặc định chỉ cho 1,25MB mỗi vùng, firmware có WiFi và OTA sẽ rất sát giới hạn.
4. `Tools → Port` chọn cổng COM vừa hiện ra.
5. Bấm nút **Upload** (mũi tên).

Nếu báo lỗi `Failed to connect`: giữ nút **BOOT** trên bo lúc nó hiện `Connecting....`, thả ra khi bắt đầu chạy.

## 4. Mở trang web

Mở `Tools → Serial Monitor`, chỉnh tốc độ **115200**. Bấm nút EN trên bo để khởi động lại, sẽ thấy:

```
Connected. Open  http://192.168.1.xx
          or  http://bomnuoc.local
```

Gõ địa chỉ đó vào trình duyệt điện thoại (điện thoại phải cùng WiFi nhà).

**Nếu sai mật khẩu WiFi** thì bo tự phát sóng WiFi riêng tên `BomNuoc-Setup`, mật khẩu `12345678`. Nối vào rồi mở `http://192.168.4.1` vẫn xem được, chỉ là chưa ra internet.

---

## 5. Thử logic bằng chế độ mô phỏng

Lúc mới nạp, firmware ở **chế độ mô phỏng**: phao và lưu lượng lấy từ các nút trên trang web chứ không đọc chân thật. **Đèn LED trên bo sáng = bơm lên bồn đang chạy.**

### Kịch bản 1 — bơm tự động
1. Bấm **Bồn đã cạn** → bơm chạy, LED sáng.
2. Kéo thanh **Lưu lượng giả lập** lên khoảng 20 → bơm vẫn chạy bình thường.
3. Bấm **Bồn đã đầy** → bơm tắt ngay, LED tắt.

### Kịch bản 2 — chống chạy cạn
1. Bấm **Xoá lỗi**, bấm bỏ *đã đầy*, để thanh lưu lượng ở **0**.
2. Bấm **Bồn đã cạn** → bơm chạy.
3. Chờ **45 giây** → bơm tự tắt, hiện băng đỏ *"chạy nhưng không có nước"*, và **khoá**.
4. Bấm **Xoá lỗi và mở khoá** để mở lại.

Khoá này được ghi vào bộ nhớ, rút điện cắm lại vẫn còn khoá — đúng như mong muốn, vì nguyên nhân chưa được xử lý.

### Kịch bản 3 — nghỉ giữa 2 lần chạy
Sau khi bồn đầy và bơm tắt, bỏ *đã đầy* rồi bấm *đã cạn* ngay. Bơm **không** chạy liền, phải đợi đủ 2 phút. Tránh bơm bật tắt liên tục khi nước sóng sánh.

### Kịch bản 4 — khoá liên động van
1. Bấm **Bật** ở thẻ Bơm áp.
2. Bấm **Chảy thường** ở thẻ Đường nước.
3. Bơm áp **tự tắt**, có thông báo. Van mở thì bơm áp không được phép chạy, nếu không nước sẽ quay vòng và bơm chạy không tải.

---

## 6. Khi đã có cảm biến thật

Bấm **Tắt mô phỏng** trên trang web, firmware chuyển sang đọc chân thật.

### Bảng chân

| Chức năng | GPIO |
|---|---|
| Relay bơm áp | 18 |
| Relay bơm lên bồn | 19 |
| Relay van bi — mở | 23 |
| Relay van bi — đóng | 13 |
| Phao mức thấp | 25 |
| Phao mức đầy | 26 |
| Lưu lượng bơm bồn | 27 |
| Lưu lượng bơm áp | 14 |
| Nút xoá lỗi | 33 |
| LED trên bo | 2 |

### Đấu relay
Bo relay 12V cắm **jumper sang kích mức thấp**. Chân ESP32 chạy chế độ open-drain: kéo xuống GND là relay hút, thả nổi là relay nhả. Nhờ vậy ESP 3.3V điều khiển được bo logic 12V.

### Đấu phao — quan trọng
Cả 2 chân phao dùng điện trở kéo lên bên trong ESP32, nên **dây đứt sẽ đọc ra mức cao**. Phải đấu sao cho mức cao luôn là trạng thái an toàn, tức *"đừng chạy bơm"*:

- **Phao mức đầy**: tiếp điểm **đóng khi chưa đầy**, mở ra khi đầy.
- **Phao mức thấp**: tiếp điểm **đóng khi nước dưới mức thấp**, mở ra khi nước lên cao.

Đấu đúng chiều này thì đứt dây hay tuột giắc sẽ làm bơm dừng, chứ không làm tràn bồn.

### Hiệu chỉnh cảm biến lưu lượng
Sửa `FLOW_PULSES_PER_LITRE` trong `config.h`. Cách đo: cho chảy đúng 10 lít vào xô, đếm số xung, chia cho 10.

---

## 7. 🔴 Lưu ý an toàn

- Giai đoạn này **chỉ chơi với 5V và 12V**. Chưa đấu 220V.
- Khi lắp thật, mỗi thiết bị giữ nguyên RCBO riêng đang có.
- Luôn giữ cầu dao tay của từng bơm, để ESP32 hỏng vẫn bật được bằng tay.
- Chân đo áp của PZEM và cuộn hút contactor phải lấy L và N **cùng một RCBO**, lấy lệch nhánh sẽ làm RCBO nhảy.

---

## 8. Cập nhật firmware qua WiFi (OTA)

Sau lần nạp đầu bằng USB, những lần sau khỏi tháo bo khỏi tủ, và **làm được từ iPhone**.

### Lấy file .bin
Trong Arduino IDE: `Sketch → Export Compiled Binary`. File `.bin` nằm trong thư mục
`build/esp32.esp32.esp32/` cạnh sketch. Lấy file **không** có đuôi `.bootloader.bin`
hay `.partitions.bin`, chỉ đúng `bom_nuoc.ino.bin`.

Gửi file đó sang điện thoại bằng AirDrop hoặc Zalo.

### Nạp
Mở trang web của bo, kéo xuống thẻ **Cập nhật firmware**: chọn file `.bin`, nhập
mật khẩu, bấm **Nạp firmware**. Bơm tự tắt trong lúc cập nhật, xong bo tự khởi động lại.

Mật khẩu đặt ở `OTA_PASSWORD` trong `config.h`, mặc định là `bomnuoc2026`.
**Nên đổi**, vì ai vào được WiFi nhà cũng mở được trang này.

### Lưu ý
- Phải chọn đúng **Partition Scheme** như mục 3, nếu không sẽ không đủ chỗ cho 2 vùng firmware.
- Đang cập nhật thì **đừng tắt nguồn**. Nạp hỏng giữa chừng thì bo vẫn giữ bản cũ, nhưng
  đừng thử vận may.
- Lần nạp **đầu tiên bắt buộc dùng máy tính**. iPhone và Android đều không nói chuyện được
  với chip USB-serial trên bo.

## 9. Điều khiển qua 4G bằng MQTT

Mặc định **tắt**, để bản đầu biên dịch được mà không cần cài thư viện. Bật khi đã sẵn sàng.

### Bước 1 — tạo broker miễn phí
Vào `hivemq.com/company/get-started` → đăng ký → tạo cụm **Serverless (free)**.
Chỉ cần email, không cần thẻ.

Trong cụm, vào **Access Management** tạo **2 tài khoản**:

| Tài khoản | Dùng cho | Quyền |
|---|---|---|
| `bomnuoc` | ESP32 | Publish & Subscribe |
| `phone` | Điện thoại | Publish & Subscribe |

Tách 2 tài khoản để lộ cái nào thì đổi riêng cái đó.

Ghi lại địa chỉ cụm, dạng `xxxxxxxx.s1.eu.hivemq.cloud`.

### Bước 2 — cài thư viện
Arduino IDE: `Tools → Manage Libraries`, tìm **PubSubClient** của **Nick O'Leary**, cài.

### Bước 3 — bật trong config.h
```c
#define USE_MQTT    1
#define MQTT_HOST   "xxxxxxxx.s1.eu.hivemq.cloud"
#define MQTT_USER   "bomnuoc"
#define MQTT_PASS   "mat_khau_cua_tai_khoan_bomnuoc"
```
Nạp lại (USB hoặc OTA). Serial Monitor sẽ in `[MQTT] connected`.

### Bước 4 — mở trang điều khiển trên điện thoại
Gửi file `dieu-khien.html` sang điện thoại (AirDrop/Zalo) rồi mở bằng trình duyệt.
Nhập địa chỉ cụm, cổng `8884`, tài khoản `phone` và mật khẩu của nó, bấm **Kết nối**.

Thông tin lưu trong trình duyệt, lần sau mở là tự nối. Dùng được mọi nơi có 4G.

### Cách hoạt động
| Topic | Chiều | Nội dung |
|---|---|---|
| `bomnuoc/state` | ESP32 → điện thoại | JSON trạng thái, có cờ retained |
| `bomnuoc/cmd` | điện thoại → ESP32 | `tank=auto`, `booster=off`, `valve=open`, `clear=1` |
| `bomnuoc/status` | ESP32 → điện thoại | `online` / `offline` |

ESP32 gửi **ngay khi trạng thái đổi**, cộng thêm nhịp tim mỗi 5 giây. Khoảng 60MB/tháng,
gói free của HiveMQ cho 10GB nên thừa rất nhiều.

`bomnuoc/status` dùng cơ chế **last will**: mất điện hay mất mạng thì broker tự báo `offline`,
nên điện thoại không bị nhìn vào màn hình cũ mà tưởng mọi thứ vẫn ổn. Trang điện thoại còn tự
cảnh báo khi quá 40 giây không nhận được số liệu mới.

### Lưu ý
- Toàn bộ **logic tự động vẫn chạy trên ESP32**. Mất internet thì bơm vẫn tự bật tắt theo phao,
  vẫn chống chạy cạn. Chỉ mất phần điều khiển từ xa.
- `MQTT_INSECURE_TLS 1` nghĩa là có mã hoá nhưng không kiểm chứng chỉ broker. Đủ dùng cho
  nhà riêng. Muốn chặt hơn thì dán root CA và đổi sang `setCACert`.

## 10. Test ảo khi chưa có phần cứng

Hai cách, bổ sung cho nhau:

| Cách | Thử được gì | Không thử được gì |
|---|---|---|
| **Chạy trên máy tính** (Python) | Toàn bộ giao diện, cả trang nội bộ lẫn trang từ xa. Logic bơm, chống chạy cạn, khoá liên động | Không chạy code C++ thật |
| **Wokwi** (trình duyệt) | Chạy đúng firmware C++, **biên dịch thật** | Giao diện web khó mở từ ngoài trình mô phỏng |

### Cách 1 — chạy trên máy tính

Chỉ cần **Python**, không cần cài thêm thư viện nào:

```
python C:\Users\pqtrung8\Projects\bom_nuoc\gia-lap\chay-thu.py
```

| Địa chỉ | Là trang gì |
|---|---|
| `http://127.0.0.1:8899/` | Trang nội bộ, y hệt trang chạy trên bo thật |
| `http://127.0.0.1:8899/tu-xa` | Trang điều khiển từ xa (`dieu-khien.html`) |

**Mở cả hai cửa sổ cạnh nhau** — chúng dùng chung một con ESP32 giả lập, bấm bên này thì
bên kia đổi theo ngay.

Thanh **màu cam** ở đầu trang từ xa đóng vai cảm biến (phao thấp, phao đầy, lưu lượng).
Bo thật không có thanh này. Trang nội bộ có thẻ **Mô phỏng cảm biến** làm việc tương tự.

Thử: bấm *Bồn đã cạn* thì bơm tự chạy và Relay 2 sáng đèn. Bật *Bơm áp* rồi bấm
*Chảy thường* thì bơm áp tự tắt. Đặt *KHÔNG có nước* rồi cho bồn cạn, chờ 45 giây thì
báo lỗi chạy cạn.

Các mốc thời gian lấy đúng theo `config.h`, không rút ngắn. Cửa sổ PowerShell in log
từng sự kiện. Dừng bằng **Ctrl + C**.

⚠️ Đây là bản mô phỏng **hành vi**, viết bằng Python. Nó kiểm chứng giao diện và luồng logic,
**không phải** code C++ thật. Muốn chắc code C++ biên dịch được thì dùng cách 2.

### Cách 2 — Wokwi

**Wokwi** chạy thẳng trên trình duyệt, miễn phí, không cần cài gì. Nó **biên dịch thật**,
nên là cách nhanh nhất để biết code có lỗi cú pháp hay không.

#### Các bước
1. Vào `wokwi.com`, bấm **New Project** → chọn **ESP32**.
2. Tab `sketch.ino`: xoá sạch, dán toàn bộ nội dung `bom_nuoc.ino`.
3. Bấm dấu **+** bên cạnh danh sách file → **New File**, đặt tên `config.h`, dán nội dung vào.
4. Làm tương tự với `page.h`.
5. Trong `config.h` sửa WiFi thành mạng ảo của Wokwi:
   ```c
   #define WIFI_SSID   "Wokwi-GUEST"
   #define WIFI_PASS   ""
   ```
6. Bấm nút **Play**. Nếu biên dịch lỗi, Wokwi hiện ngay dòng lỗi.

File `wokwi/diagram.json` trong thư mục này là bản mạch tối thiểu, dán vào tab `diagram.json`
của Wokwi nếu cần.

### Điều khiển bằng Serial Monitor
Trang web nội bộ có thể không mở được từ bên ngoài trình mô phỏng. Không sao — firmware có sẵn
bảng lệnh qua Serial Monitor, gõ lệnh rồi Enter:

| Lệnh | Tác dụng |
|---|---|
| `s` | Xem trạng thái |
| `?` | Danh sách lệnh |
| `simAboveLow=0` | Giả lập bồn đã cạn |
| `simIsFull=1` | Giả lập bồn đã đầy |
| `simFlow=20` | Đặt lưu lượng giả lập, L/phút |
| `tank=auto` / `on` / `off` | Chế độ bơm lên bồn |
| `booster=on` / `off` | Bơm áp |
| `valve=open` / `close` | Van bi |
| `clear=1` | Xoá lỗi và mở khoá |

Nối nhiều lệnh bằng `&`, ví dụ `simIsFull=0&simAboveLow=0`.

### Kịch bản thử trên Wokwi
```
simFlow=20          -> dat luu luong binh thuong
simAboveLow=0       -> bon can, bom chay, LED sang
simIsFull=1         -> bon day, bom tat ngay

clear=1
simIsFull=0&simFlow=0
simAboveLow=0       -> bom chay nhung khong co nuoc
                       cho 45 giay, bom tu tat va bao loi chay can
clear=1             -> mo khoa

booster=on
valve=open          -> bom ap tu tat vi van bypass mo
```

Cùng bộ lệnh này dùng được luôn trên bo thật khi cắm USB, tiện để dò lỗi.

## 11. Đo nhiệt độ nước bằng DS18B20

Mặc định **tắt**. Bật khi đã có cảm biến.

### Linh kiện
- **DS18B20 chống nước** (vỏ inox, có dây) — khoảng 40–60k một con
- **1 điện trở 4,7 kΩ** — chỉ cần đúng 1 con cho cả đường dây, dù gắn mấy cảm biến

### Đấu dây
Nhiều cảm biến dùng **chung một sợi dây, chung một chân ESP32**. Cứ nối song song cả 3 dây.

| Dây DS18B20 | Nối vào |
|---|---|
| Đỏ (VDD) | 3,3V của ESP32 |
| Đen hoặc xanh (GND) | GND |
| Vàng (DATA) | **GPIO 4** |

Thêm **điện trở 4,7 kΩ nối giữa dây vàng và 3,3V**. Thiếu con này là không đọc được gì cả.

Đừng dùng kiểu cấp nguồn ký sinh (chỉ 2 dây), chạy đủ 3 dây cho chắc.

Dây dài dưới 10 m thì dùng dây thường được. Dài hơn nên dùng cáp mạng có chống nhiễu và
đổi điện trở xuống 2,2 kΩ.

### Gắn ở đâu
- **Bồn mái** — thả cảm biến vào trong bồn, hoặc kẹp vào ống ra rồi bọc xốp cách nhiệt.
- **Bình nóng lạnh** — kẹp vào **ống nước nóng đi ra** rồi bọc xốp. Không cần chạm nước.
  Nhiệt độ đo được thấp hơn trong bình vài độ nhưng đủ biết bình đã nóng chưa.

### Bật trong firmware
1. `Tools → Manage Libraries`, cài **OneWire** (Paul Stoffregen) và
   **DallasTemperature** (Miles Burton).
2. Trong `config.h` đổi `USE_TEMPERATURE` thành `1`.
3. Nạp lại. Serial Monitor sẽ in `[TEMP] found N sensor(s)`.

### Đặt đúng tên cho từng cảm biến
Thư viện xếp cảm biến theo **địa chỉ chip**, không theo thứ tự bạn cắm dây. Nên con số 0 chưa
chắc đã là cái bạn nghĩ.

Cách xác định: gõ `s` trong Serial Monitor để xem danh sách, lấy tay nắm ấm một đầu dò,
chờ chục giây rồi gõ `s` lại. Chỉ số nào tăng lên thì đó là con đó. Sau đó đổi
`TEMP_NAME_0`, `TEMP_NAME_1`, `TEMP_NAME_2` trong `config.h` cho khớp.

### Lưu ý kỹ thuật
Một lần đo của DS18B20 mất khoảng 750 ms. Firmware **không chờ** — nó ra lệnh đo rồi quay lại
việc khác, tới nhịp sau mới lấy kết quả. Nếu chờ thẳng thì web và logic bơm sẽ đứng gần một
giây mỗi lần đo.

Mặc định đo lại mỗi 10 giây, đổi ở `TEMP_READ_MS`.

## 12. Chưa có trong bản này

- Đo mức nước ra phần trăm (bật cờ `USE_LEVEL_SENSOR`, hàm quy đổi bồn nằm đã viết sẵn)
- Đo điện PZEM-004T
- WebSocket thay cho polling 1 giây

Làm tiếp từng phần sau khi bản này chạy ổn.
