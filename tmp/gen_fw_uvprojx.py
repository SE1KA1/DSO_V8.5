# -*- coding: utf-8 -*-
"""从主 project.uvprojx 生成 firmware 精简工程 uvprojx（ElementTree 外科手术）。"""
import xml.etree.ElementTree as ET
import os

SRC = r'D:\DSO_V8.5\Project\MDK-ARM(uV5)\project.uvprojx'
DST = r'D:\DSO_V8.5\module_telemetry_delivery\firmware\Project\MDK-ARM(uV5)\project.uvprojx'
FW  = r'D:\DSO_V8.5\module_telemetry_delivery\firmware'

KEEP_GROUPS = {'MDK-ARM', 'HAL_Driver', 'CMSIS', 'FreeRTOS/Source',
               'FreeRTOS/Ports', 'LwIP_Core'}
DROP_GROUPS = {'SEGGER/HardFault', 'Doc', 'UI', 'emwin'}

# DSO 组：仅保留这些 basename（其余 58 个 .c 删除）
DSO_KEEP_BASE = {
    'dso_module_upload.c', 'dso_switch_panel.c', 'dso_sht40.c',
    'dso_ina226.c', 'dso_ina226_u44.c', 'dso_watchdog.c',
}
# 新增文件（交付树独有）
DSO_ADD = [('module_5ch_stub.c', r'..\..\User\dso\module_5ch_stub.c')]

# User 组保留：main.c + FreeRTOSConfig.h
USER_KEEP_BASE = {'main.c', 'FreeRTOSConfig.h'}

# BSP 组保留 basename
BSP_KEEP_BASE = {
    'bsp.c', 'bsp.h', 'bsp_dwt.c', 'bsp_led.c', 'bsp_uart_fifo.c',
    'bsp_i2c_ui.c', 'bsp_pcal6524.c', 'stm32h7xx_it.c',
}

# LWIP 组保留 basename
LWIP_KEEP_BASE = {'lwip.c', 'ethernetif.c', 'sys_arch.c', 'lan8742.c'}

DEFINES = 'USE_HAL_DRIVER, STM32H743xx'
INCPATH = (
    r'..\..\Libraries\CMSIS\Include'
    r';..\..\Libraries\CMSIS\Device\ST\STM32H7xx\Include'
    r';..\..\Libraries\STM32H7xx_HAL_Driver\Inc'
    r';..\..\User\bsp\inc'
    r';..\..\User'
    r';..\..\User\bsp'
    r';..\..\FreeRTOS\include'
    r';..\..\FreeRTOS\portable\RVDS\ARM_CM7\r0p1'
    r';..\..\LWIP\App'
    r';..\..\LWIP\Target'
    r';..\..\Middlewares\Third_Party\LwIP\src\include'
    r';..\..\Middlewares\Third_Party\LwIP\system'
    r';..\..\Drivers\BSP\Components\lan8742'
    r';..\..\Middlewares\Third_Party\FreeRTOS\Source\CMSIS_RTOS'
)


def base_of_fp(fp):
    return os.path.basename(fp.replace('\\', '/'))


def tag(e):
    return e.tag.split('}')[-1]


def find_group(groups_el, name):
    for grp in groups_el:
        if tag(grp) != 'Group':
            continue
        nm = None
        for c in grp:
            if tag(c) == 'GroupName':
                nm = c.text
        if nm == name:
            return grp
    return None


def main():
    t = ET.parse(SRC)
    root = t.getroot()
    targets = None
    for ch in root:
        if tag(ch) == 'Targets':
            targets = ch
    assert targets is not None and len(list(targets)) == 1
    tgt = list(targets)[0]
    groups_el = None
    for c in tgt:
        if tag(c) == 'Groups':
            groups_el = c
    assert groups_el is not None

    # ---- 1) 处理各保留组内的 File 子集 + 删整组 ----
    for grp in list(groups_el):
        nm = None
        for c in grp:
            if tag(c) == 'GroupName':
                nm = c.text
        if nm in DROP_GROUPS:
            groups_el.remove(grp)
            print('drop group:', nm)
            continue
        if nm in KEEP_GROUPS:
            print('keep group verbatim:', nm)
            continue
        if nm == 'User':
            for files_el in grp:
                if tag(files_el) != 'Files':
                    continue
                for f in list(files_el):
                    if tag(f) != 'File':
                        continue
                    fp = None
                    for x in f:
                        if tag(x) == 'FilePath':
                            fp = x.text
                    keep = (fp is not None and base_of_fp(fp) in USER_KEEP_BASE)
                    if not keep:
                        files_el.remove(f)
                        print('  user del:', fp)
            continue
        if nm == 'DSO':
            for files_el in grp:
                if tag(files_el) != 'Files':
                    continue
                for f in list(files_el):
                    if tag(f) != 'File':
                        continue
                    fp = None
                    for x in f:
                        if tag(x) == 'FilePath':
                            fp = x.text
                    keep = (fp is not None and base_of_fp(fp) in DSO_KEEP_BASE)
                    if not keep:
                        files_el.remove(f)
                        print('  dso del:', fp)
                # append new files
                import copy
                for fname, fp in DSO_ADD:
                    f_el = ET.SubElement(files_el, 'File')
                    ET.SubElement(f_el, 'FileName').text = fname
                    ET.SubElement(f_el, 'FileType').text = '1'
                    ET.SubElement(f_el, 'FilePath').text = fp
                    print('  dso add:', fp)
            # rename group DSO -> App_Delivery
            for c in grp:
                if tag(c) == 'GroupName':
                    c.text = 'App_Delivery'
            continue
        if nm == 'BSP':
            for files_el in grp:
                if tag(files_el) != 'Files':
                    continue
                for f in list(files_el):
                    if tag(f) != 'File':
                        continue
                    fp = None
                    for x in f:
                        if tag(x) == 'FilePath':
                            fp = x.text
                    keep = (fp is not None and base_of_fp(fp) in BSP_KEEP_BASE)
                    if not keep:
                        files_el.remove(f)
                        print('  bsp del:', fp)
            continue
        if nm == 'LWIP':
            for files_el in grp:
                if tag(files_el) != 'Files':
                    continue
                for f in list(files_el):
                    if tag(f) != 'File':
                        continue
                    fp = None
                    for x in f:
                        if tag(x) == 'FilePath':
                            fp = x.text
                    keep = (fp is not None and base_of_fp(fp) in LWIP_KEEP_BASE)
                    if not keep:
                        files_el.remove(f)
                        print('  lwip del:', fp)
            continue
        print('!! unhandled group:', nm)

    # ---- 2) TargetOption 编辑：Define / IncludePath / DSP lib / AfterMake / OutputName ----
    target_opts = None
    for c in tgt:
        if tag(c) == 'TargetOption':
            target_opts = c

    def find_desc(el, name):
        for d in el.iter():
            if tag(d) == name:
                return d
        return None

    # 2a) TargetCommonOption: OutputName -> telemetry（不动其 IncludePath）
    tgt_common = None
    for c in target_opts:
        if tag(c) == 'TargetCommonOption':
            tgt_common = c
    if tgt_common is not None:
        outname = find_desc(tgt_common, 'OutputName')
        if outname is not None:
            print('old OutputName:', outname.text)
            outname.text = 'telemetry'

    # 2b) TargetArmAds/Cads/VariousControls: Define + IncludePath（C 编译器实际配置）
    for armads in target_opts:
        if tag(armads) != 'TargetArmAds':
            continue
        for cads in armads:
            if tag(cads) != 'Cads':
                continue
            for vc in cads:
                if tag(vc) != 'VariousControls':
                    continue
                define_el = find_desc(vc, 'Define')
                if define_el is not None:
                    print('old C Define:', define_el.text)
                    define_el.text = DEFINES
                inc_el = find_desc(vc, 'IncludePath')
                if inc_el is not None:
                    inc_el.text = INCPATH

    # 2c) 删 IncludeLibs / IncludeLibsPath（DSP 库）
    for rm_tag in ('IncludeLibs', 'IncludeLibsPath'):
        el = find_desc(target_opts, rm_tag)
        if el is not None:
            par = None
            for p in target_opts.iter():
                if el in list(p):
                    par = p
                    break
            if par is not None:
                par.remove(el)
                print('removed', rm_tag)
    # 2d) AfterMake RunUserProg1=0
    am = find_desc(target_opts, 'AfterMake')
    if am is not None:
        for c in am.iter():
            if tag(c) == 'RunUserProg1':
                c.text = '0'
            if tag(c) == 'UserProg1Name':
                c.text = ''

    # ---- 3) 写 ----
    tree = ET.ElementTree(root)
    ET.register_namespace('', '')
    # uvprojx 用默认命名空间? 原文件无命名空间。直接写即可。
    # ElementTree 会写 XML 声明 utf-8
    import io
    buf = io.BytesIO()
    tree.write(buf, encoding='utf-8', xml_declaration=True)
    with open(DST, 'wb') as fh:
        fh.write(buf.getvalue())
    print('written:', DST)

    # ---- 4) 校验：解析 DST + 检查每个保留 File 在 FW 树存在 ----
    t2 = ET.parse(DST)
    missing = []
    total_files = 0
    for grp in t2.getroot().iter():
        if tag(grp) != 'Group':
            continue
        for files_el in grp:
            if tag(files_el) != 'Files':
                continue
            for f in files_el:
                if tag(f) != 'File':
                    continue
                fp = None
                for x in f:
                    if tag(x) == 'FilePath':
                        fp = x.text
                if fp:
                    total_files += 1
                    rel = fp.replace('..\\..\\', '')
                    p = os.path.join(FW, rel.replace('/', '\\'))
                    if not os.path.exists(p):
                        missing.append(fp)
    print('total File entries:', total_files)
    if missing:
        print('!! MISSING in firmware tree:')
        for m in missing:
            print('   ', m)
    else:
        print('all File entries exist in firmware tree OK')


if __name__ == '__main__':
    main()
