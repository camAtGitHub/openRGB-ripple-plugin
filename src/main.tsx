import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import { Studio } from "@/components/studio";
import "@/styles.css";

const root = document.getElementById("root");
if (!root) throw new Error("Missing #root");

createRoot(root).render(
  <StrictMode>
    <Studio />
  </StrictMode>,
);
