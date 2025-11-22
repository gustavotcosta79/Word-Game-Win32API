# Word-Game-Win32API

This project is part of a **multi-user game system** developed in **C/C++ for Windows**, using **shared memory** for inter-process communication.  
The panel displays real-time information about the game state, including the last word played, visible letters, and the current player rankings.

---

## 🧩 Project Components

### **arbitro**
The main game controller process responsible for managing the entire game logic.

### **jogadorUI**
Graphical user interface for human players.

### **bot**
Automated player process that interacts with the game like a real user.

### **painel**
Graphical panel that displays real-time game information to observers.

### **shared**
Shared codebase containing common structures, definitions, and utility functions used by all processes.

---

## ▶️ How to Run

1. Start the **arbitro** process.  
2. Launch one or several **jogadorUI** or **bot** processes.  
3. Run the **painel** to visualize the game state in real time.  

> **Note:** All processes must run on the **same machine** to access the shared memory segment.
