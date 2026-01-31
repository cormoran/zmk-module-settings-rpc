import type { Config } from "jest";

const config: Config = {
  preset: "ts-jest",
  testEnvironment: "jsdom",
  setupFilesAfterEnv: ["<rootDir>/test/setup.ts"],
  moduleNameMapper: {
    // Map the base paths first
    "^@zmkfirmware/zmk-studio-ts-client$":
      "<rootDir>/node_modules/@zmkfirmware/zmk-studio-ts-client/lib/index.js",
    // Map the specific module paths
    "^@zmkfirmware/zmk-studio-ts-client/transport/serial$":
      "<rootDir>/node_modules/@zmkfirmware/zmk-studio-ts-client/lib/transport/serial.js",
    "^@zmkfirmware/zmk-studio-ts-client/(.*)$":
      "<rootDir>/node_modules/@zmkfirmware/zmk-studio-ts-client/lib/$1.js",
    "^@cormoran/zmk-studio-react-hook/testing$":
      "<rootDir>/node_modules/@cormoran/zmk-studio-react-hook/lib/testing/index.js",
    "^@cormoran/zmk-studio-react-hook$":
      "<rootDir>/node_modules/@cormoran/zmk-studio-react-hook/lib/index.js",
    // Mock CSS imports
    "\\.(css|less|scss|sass)$": "identity-obj-proxy",
  },
  transformIgnorePatterns: ["node_modules/(?!(@cormoran|@zmkfirmware)/)"],
  transform: {
    "^.+\\.(js|jsx|ts|tsx)$": [
      "ts-jest",
      {
        tsconfig: {
          jsx: "react-jsx",
          esModuleInterop: true,
          allowSyntheticDefaultImports: true,
        },
        isolatedModules: true,
        diagnostics: false,
      },
    ],
  },
  testMatch: ["**/test/**/*.spec.ts", "**/test/**/*.spec.tsx"],
};

export default config;
