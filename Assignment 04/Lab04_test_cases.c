/* ===============================================================================
 * TEST HARNESS- Comment out the real main() in main.c & paste this in its place
 *
 * Uses a menu over UART: send '1'..'9' or 'a','b','c' to run TC1-TC12.
 * =============================================================================== */

int main(void)
{
  HAL_Init();
  FPU_Enable();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_USART2_UART_Init();

  UART_SendStr("\r\n--- TEST HARNESS: send a key to run a test case ---\r\n");
  UART_SendStr("1: TC1  raw ADC responsiveness (streams raw code for 10s)\r\n");
  UART_SendStr("2: TC2  flash erase verification (Sector 7)\r\n");
  UART_SendStr("3: TC3  flash write/read-back (0xDEADBEEF @ Sector 7 base)\r\n");
  UART_SendStr("4: TC4  identity provisioning (EDIT NAMES BELOW FIRST!)\r\n");
  UART_SendStr("5: TC5  identity display (just calls Identity_Display)\r\n");
  UART_SendStr("6: TC6  run test suite N times, then show identity\r\n");
  UART_SendStr("7: TC7  show Results_Display (check blank/fallback)\r\n");
  UART_SendStr("8: TC8  run test suite while logging every raw sample\r\n");
  UART_SendStr("9: TC9  run test suite twice back-to-back, then Results_Display\r\n");
  UART_SendStr("a: TC10 print all 4 resolutions' volts for current pot position\r\n");
  UART_SendStr("b: TC11 debounce test (prints run count after burst)\r\n");
  UART_SendStr("c: TC12 just prints identity + results (use after power-cycle)\r\n");

  while (1)
  {
    uint8_t key;
    HAL_UART_Receive(&huart2, &key, 1, HAL_MAX_DELAY);

    char line[80];

    switch (key)
    {
      /* ---------------- TC1: raw ADC responsiveness ---------------- */
      case '1':
      {
        UART_SendStr("\r\n[TC1] Streaming raw 12-bit code for ~10s. Rotate POT now.\r\n");
        ADC_SetResolution(RES_12BIT);
        for (int i = 0; i < 200; i++) /* ~10s at 50ms/sample */
        {
          uint16_t raw = ADC_ReadRaw();
          int len = snprintf(line, sizeof(line), "raw=%u\r\n", raw);
          HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
          HAL_Delay(50);
        }
        UART_SendStr("[TC1] Done.\r\n");
        break;
      }

      /* ---------------- TC2: flash erase verification ---------------- */
      case '2':
      {
        UART_SendStr("\r\n[TC2] Erasing Sector 7...\r\n");
        Flash_EraseSector(FLASH_SECTOR7_NUM);
        uint32_t val = *(volatile uint32_t *)FLASH_SECTOR7_BASE;
        int len = snprintf(line, sizeof(line), "Read back: 0x%08lX (expect 0xFFFFFFFF)\r\n",
                            (unsigned long)val);
        HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
        break;
      }

      /* ---------------- TC3: flash write/read-back ---------------- */
      case '3':
      {
        UART_SendStr("\r\n[TC3] Erasing Sector 7, writing 0xDEADBEEF...\r\n");
        Flash_EraseSector(FLASH_SECTOR7_NUM);
        Flash_WriteWord(FLASH_SECTOR7_BASE, 0xDEADBEEFU);
        uint32_t val = *(volatile uint32_t *)FLASH_SECTOR7_BASE;
        int len = snprintf(line, sizeof(line), "Read back: 0x%08lX (expect 0xDEADBEEF)\r\n",
                            (unsigned long)val);
        HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
        UART_SendStr("[TC3] NOTE: Sector 7 now holds test junk, not a valid results block.\r\n");
        UART_SendStr("      Run a real test suite (case '6' or '9') afterward to restore it.\r\n");
        break;
      }

      /* ---------------- TC4: identity provisioning ---------------- */
      case '4':
      {
        UART_SendStr("\r\n[TC4] Provisioning identity block...\r\n");
        /* EDIT THESE VALUES BEFORE RUNNING, THEN RE-COMMENT/REMOVE AFTER USE. */
//        Identity_ProvisionPair("2023-915-945", "2", "Md. Samiul Islam Siam",
//                               "2023-315-950", "7", "Partho Kumar Mondal");
        UART_SendStr("[TC4] Provisioning done. Reading back:\r\n");
        Identity_Display();
        break;
      }

      /* ---------------- TC5: identity display at boot ---------------- */
      case '5':
      {
        UART_SendStr("\r\n[TC5] Current identity block contents:\r\n");
        Identity_Display();
        UART_SendStr("[TC5] Now RESET the board (not this menu) and confirm this\r\n");
        UART_SendStr("      is the first thing printed, before results/prompt.\r\n");
        break;
      }

      /* ---------------- TC6: identity survives repeated testing ---------------- */
      case '6':
      {
        UART_SendStr("\r\n[TC6] Identity BEFORE running test suite 3x:\r\n");
        Identity_Display();
        for (int i = 0; i < 3; i++)
        {
          int len = snprintf(line, sizeof(line), "\r\n-- Test run %d of 3 --\r\n", i + 1);
          HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
          RunTestSuite();
        }
        UART_SendStr("\r\n[TC6] Identity AFTER running test suite 3x (should be unchanged):\r\n");
        Identity_Display();
        break;
      }

      /* ---------------- TC7: blank results fallback ---------------- */
      case '7':
      {
        UART_SendStr("\r\n[TC7] Current results block:\r\n");
        Results_Display();
        UART_SendStr("[TC7] To test the blank case: run TC2 (erase Sector 7 only),\r\n");
        UART_SendStr("      reset the board, and confirm this shows 'No previous test data.'\r\n");
        break;
      }

      /* ---------------- TC8: per-resolution averaging with raw log ---------------- */
      case '8':
      {
        UART_SendStr("\r\n[TC8] Running test suite with raw sample log per resolution.\r\n");
        for (Resolution_t r = RES_12BIT; r < RES_COUNT; r++)
        {
          ADC_SetResolution(r);
          HAL_Delay(1);

          int lenh = snprintf(line, sizeof(line), "-- %s --\r\n", kResTable[r].label);
          HAL_UART_Transmit(&huart2, (uint8_t *)line, lenh, HAL_MAX_DELAY);

          uint32_t sum = 0;
          for (int s = 0; s < N_SAMPLES; s++)
          {
            uint16_t raw = ADC_ReadRaw();
            sum += raw;
            int lens = snprintf(line, sizeof(line), "  sample[%d]=%u\r\n", s, raw);
            HAL_UART_Transmit(&huart2, (uint8_t *)line, lens, HAL_MAX_DELAY);
          }
          uint32_t avgCode = sum / N_SAMPLES;
          float volts = CodeToVolts(avgCode, kResTable[r].maxcode);
          int lenv = snprintf(line, sizeof(line), "  computed avg_code=%lu  V=%.3f\r\n",
                               (unsigned long)avgCode, volts);
          HAL_UART_Transmit(&huart2, (uint8_t *)line, lenv, HAL_MAX_DELAY);
        }
        ADC_SetResolution(RES_12BIT);
        UART_SendStr("[TC8] Compare each 'computed avg' above to the value RunTestSuite()\r\n");
        UART_SendStr("      would print/store for the same resolution (run case '9' next).\r\n");
        break;
      }

      /* ---------------- TC9: result overwrite on re-test ---------------- */
      case '9':
      {
        UART_SendStr("\r\n[TC9] Running test suite (1st run). Move POT after this completes.\r\n");
        RunTestSuite();
        UART_SendStr("[TC9] Send any key when ready to run the 2nd test...\r\n");
        uint8_t dummy;
        HAL_UART_Receive(&huart2, &dummy, 1, HAL_MAX_DELAY);
        UART_SendStr("[TC9] Running test suite (2nd run)...\r\n");
        RunTestSuite();
        UART_SendStr("[TC9] Now RESET the board and confirm Results_Display() shows\r\n");
        UART_SendStr("      only the 2nd run's voltages.\r\n");
        break;
      }

      /* ---------------- TC10: resolution boundary sanity ---------------- */
      case 'a':
      {
        UART_SendStr("\r\n[TC10] Sampling all 4 resolutions at current POT position:\r\n");
        for (Resolution_t r = RES_12BIT; r < RES_COUNT; r++)
        {
          ADC_SetResolution(r);
          HAL_Delay(1);
          uint32_t sum = 0;
          for (int s = 0; s < N_SAMPLES; s++)
          {
            sum += ADC_ReadRaw();
          }
          uint32_t avgCode = sum / N_SAMPLES;
          float volts = CodeToVolts(avgCode, kResTable[r].maxcode);
          int len = snprintf(line, sizeof(line), "%s: V=%.3f\r\n", kResTable[r].label, volts);
          HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
        }
        ADC_SetResolution(RES_12BIT);
        UART_SendStr("[TC10] Confirm all 4 values are close (within ~1 LSB of 6-bit step).\r\n");
        break;
      }

      /* ---------------- TC11: UART auto-trigger / debounce reliability ---------------- */
      case 'b':
      {
        UART_SendStr("\r\n[TC11] Send a SINGLE keypress now (already consumed to enter\r\n");
        UART_SendStr("        this test). Debouncing and waiting for burst to end...\r\n");
        HAL_Delay(UART_DEBOUNCE_MS);
        int drained = 0;
        uint8_t b;
        while (HAL_UART_Receive(&huart2, &b, 1, 1) == HAL_OK)
        {
          drained++;
          HAL_Delay(UART_DEBOUNCE_MS);
        }
        int len = snprintf(line, sizeof(line),
                            "[TC11] Drained %d extra byte(s) after debounce window.\r\n", drained);
        HAL_UART_Transmit(&huart2, (uint8_t *)line, len, HAL_MAX_DELAY);
        UART_SendStr("[TC11] Repeat: press once -> expect 0 drained.\r\n");
        UART_SendStr("       Then mash several keys quickly -> expect >0 drained, but this\r\n");
        UART_SendStr("       whole case should still only execute ONCE per burst.\r\n");
        break;
      }

      /* ---------------- TC12: power-cycle persistence ---------------- */
      case 'c':
      {
        UART_SendStr("\r\n[TC12] Identity + Results as read at this boot:\r\n");
        Identity_Display();
        Results_Display();
        UART_SendStr("[TC12] Unplug USB, replug, and confirm both print correctly again\r\n");
        UART_SendStr("       WITHOUT running this menu (i.e. check normal main() boot output,\r\n");
        UART_SendStr("       or re-flash this test harness and check right after reset).\r\n");
        break;
      }

      default:
        UART_SendStr("\r\nUnknown key. Send 1-9, a, b, or c.\r\n");
        break;
    }

    UART_SendStr("\r\n--- Ready for next test key ---\r\n");
  }
}