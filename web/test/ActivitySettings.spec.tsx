/**
 * Tests for ActivitySettings component
 *
 * This test demonstrates how to use react-zmk-studio test helpers to test
 * components that interact with ZMK devices.
 */

import { render, screen } from "@testing-library/react";
import {
  createConnectedMockZMKApp,
  ZMKAppProvider,
} from "@cormoran/zmk-studio-react-hook/testing";
import {
  ActivitySettings,
  SUBSYSTEM_IDENTIFIER,
} from "../src/ActivitySettings";

describe("ActivitySettings Component", () => {
  describe("With Subsystem", () => {
    it("should render activity settings controls when subsystem is found", () => {
      // Create a connected mock ZMK app with the required subsystem
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <ActivitySettings autoFetch={false} />
        </ZMKAppProvider>
      );

      // Check for activity settings UI elements
      expect(
        screen.getByText(/Activity Settings \(Sleep\/Idle Timeout\)/i)
      ).toBeInTheDocument();
      expect(
        screen.getByText(
          /Configure how long the keyboard waits before going idle or to sleep/i
        )
      ).toBeInTheDocument();
      expect(
        screen.getByLabelText(/Idle Timeout \(ms\):/i)
      ).toBeInTheDocument();
      expect(
        screen.getByLabelText(/Sleep Timeout \(ms\):/i)
      ).toBeInTheDocument();
      expect(screen.getByText(/Refresh/i)).toBeInTheDocument();
      expect(screen.getByText(/Save Settings/i)).toBeInTheDocument();
    });

    it("should show default input values", () => {
      const mockZMKApp = createConnectedMockZMKApp({
        subsystems: [SUBSYSTEM_IDENTIFIER],
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <ActivitySettings autoFetch={false} />
        </ZMKAppProvider>
      );

      // Check that the inputs have default values
      const idleInput = screen.getByLabelText(
        /Idle Timeout \(ms\):/i
      ) as HTMLInputElement;
      expect(idleInput.value).toBe("0");

      const sleepInput = screen.getByLabelText(
        /Sleep Timeout \(ms\):/i
      ) as HTMLInputElement;
      expect(sleepInput.value).toBe("0");
    });
  });

  describe("Without Subsystem", () => {
    it("should show warning when subsystem is not found", () => {
      // Create a connected mock ZMK app without the required subsystem
      const mockZMKApp = createConnectedMockZMKApp({
        deviceName: "Test Device",
        subsystems: [], // No subsystems
      });

      render(
        <ZMKAppProvider value={mockZMKApp}>
          <ActivitySettings autoFetch={false} />
        </ZMKAppProvider>
      );

      // Check for warning message
      expect(
        screen.getByText(/Subsystem "zmk__settings" not found/i)
      ).toBeInTheDocument();
      expect(
        screen.getByText(
          /Make sure your firmware includes the settings module/i
        )
      ).toBeInTheDocument();
    });
  });

  describe("Without ZMKAppContext", () => {
    it("should not render when ZMKAppContext is not provided", () => {
      const { container } = render(<ActivitySettings autoFetch={false} />);

      // Component should return null when context is not available
      expect(container.firstChild).toBeNull();
    });
  });
});
