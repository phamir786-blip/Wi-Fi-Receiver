Import("env")
import os
import subprocess

def merge_bin_action(source, target, env):
    build_dir = env.subst("$BUILD_DIR")
    firmware_bin = os.path.join(build_dir, "firmware.bin")
    bootloader_bin = os.path.join(build_dir, "bootloader.bin")
    partitions_bin = os.path.join(build_dir, "partitions.bin")
    merged_bin = os.path.join(build_dir, "firmware-merged.bin")

    # Locate boot_app0.bin from Arduino-ESP32 package
    platform = env.PioPlatform()
    framework_dir = platform.get_package_dir("framework-arduinoespressif32") or ""
    boot_app0 = os.path.join(framework_dir, "tools", "partitions", "boot_app0.bin")

    flash_mode = env.get("BOARD_FLASH_MODE", "dio")
    flash_freq = str(env.get("BOARD_F_FLASH", "40m")).replace("000000L", "m").replace("000000", "m")
    flash_size = env.get("BOARD_FLASH_SIZE", "4MB")

    cmd = [
        env.subst("$PYTHONEXE"),
        "-m",
        "esptool",
        "--chip",
        "esp32c3",
        "merge_bin",
        "-o",
        merged_bin,
        "--flash_mode",
        flash_mode,
        "--flash_freq",
        flash_freq,
        "--flash_size",
        flash_size,
        "0x0",
        bootloader_bin,
        "0x8000",
        partitions_bin,
    ]

    if os.path.isfile(boot_app0):
        cmd.extend(["0xe000", boot_app0])

    cmd.extend(["0x10000", firmware_bin])

    print("\n--------------------------------------------------")
    print(f"Generating merged ESP32-C3 firmware binary: {merged_bin}")
    try:
        subprocess.run(cmd, check=True)
        print("Merged binary generated successfully! (Flash directly at 0x0)")
    except Exception as e:
        print(f"Warning: Could not run esptool merge_bin: {e}")
    print("--------------------------------------------------\n")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", merge_bin_action)
