# ESPWifiConfig

**ESPWifiConfig** es una librería para ESP32 desarrollada con ESP-IDF que permite configurar fácilmente la conexión Wi-Fi a través de una interfaz web integrada.  
Es una solución moderna y ligera pensada para dispositivos IoT que necesitan ser configurados por el usuario final sin depender de herramientas externas.

---

## ¿Qué hace ESPWifiConfig?

Cuando el ESP32 no tiene credenciales Wi-Fi almacenadas o no logra conectarse a una red conocida:

1. Inicia un punto de acceso Wi-Fi (AP) con un portal web embebido.
2. El usuario se conecta a ese AP desde su teléfono o PC.
3. Accede a la interfaz en el navegador (`http://192.168.4.1`).
4. Ingresa el SSID y la contraseña de su red Wi-Fi.
5. El dispositivo intenta conectarse como cliente (modo STA).
6. Si logra conectarse:
   - Detiene el AP y el servidor web.
   - Almacena las credenciales en la memoria NVS.
7. Si falla, vuelve a mostrar el portal para intentar de nuevo.

---

## ¿Por qué usar ESPWifiConfig?

- ⚡ **Rápido**: inicia automáticamente en modo configuración si no hay red.
- 🌐 **Intuitivo**: permite al usuario configurar la red desde cualquier navegador.
- 🔁 **Robusto**: maneja reconexión automática y timeout configurable.
- 💾 **Persistente**: guarda las credenciales usando NVS.
- 📦 **100% ESP-IDF**: no requiere librerías externas ni hacks de Arduino.

---

## Casos de uso

- Dispositivos IoT que se instalan en lugares con redes Wi-Fi variables
- Productos de consumo que necesitan ser configurados fácilmente por usuarios sin conocimientos técnicos
- Proyectos que buscan evitar configuraciones manuales en el firmware

---

> ESPWifiConfig es un proyecto en desarrollo activo.  
> Próximamente: escaneo de redes, modo componente reutilizable y portal cautivo real con redirección automática.

---

📌 Desarrollado por **Diego Valle**  
