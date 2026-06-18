import ReactDOM from "react-dom/client";
import App from "./App";
import "./styles/global.css";

document.addEventListener("contextmenu", (e) => {
  e.preventDefault();
});

document.addEventListener("dragstart", (e) => {
  e.preventDefault();
});

ReactDOM.createRoot(document.getElementById("root")!).render(<App />);
