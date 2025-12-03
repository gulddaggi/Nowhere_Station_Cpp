# Nowhere Station

> **망자들의 마지막 역을 운영하는 메타버스형 3D 협동 시뮬레이션**
> *(Powered by UE5, PlayFab MPS, Azure Functions)*

![Project Status](https://img.shields.io/badge/Project_Status-In_Development-orange)
![Engine](https://img.shields.io/badge/Engine-Unreal_Engine_5-black)
![Platform](https://img.shields.io/badge/Platform-PC_(Windows)-blue)
![Multiplayer](https://img.shields.io/badge/Multiplayer-Dedicated_Server_(UDP)-green)

---

## 📅 Project Info

| Category | Details |
| :--- | :--- |
| **장르** | 3D 협동 시뮬레이션 (Co-op Simulation) |
| **개발 기간** | **2025.08.31 ~ 2025.10.03 (5주)** |
| **개발 인원** | 6명 (기획 1, 클라이언트 4, 서버 1) |
| **담당 역할** | **Unreal Engine Client & Server Developer** |
| **주요 업무** | 클라이언트 로직 구현, BP → C++ 이식, C++ Dedicated Server 멀티플레이 로직 구현 |
| **개발 환경** | Unreal Engine 5.6.1, Visual Studio 2022, GitLab, PlayFab |
| **출시 현황** | **Steam, Stove Demo 출시 후 운영 중** |

---

## 📖 1. Overview

**Nowhere Station**은 현실과 사후 세계의 경계, 끝없는 바다 위에 떠 있는 기차역을 배경으로 하는 **메타버스형 3D 협동 운영 시뮬레이션 게임**입니다.

플레이어는 과거의 기억을 잃고 가면을 쓴 역무원이 되어, 동료들과 함께 **청소, 수리, 발권, 보안** 업무를 수행하며 망자들의 마지막 여행을 돕습니다. 단순한 UI 클릭형 타이쿤 게임이 아닌, **직접 3D 공간을 뛰어다니며 상호작용하는 메타버스 경험**을 지향합니다.

### 🎯 Key Goals
- **Immersive Operation:** 메뉴가 아닌, 청소기·스패너·스캐너 등을 직접 들고 뛰는 3인칭 액션
- **Co-op & Metaverse:** 여러 플레이어가 하나의 역 공간(허브)을 공유하고 역할을 분담
- **Server Orchestration:** PlayFab MPS와 Azure Functions를 활용한 안정적인 데디케이티드 서버(DS) 할당 자동화

---

## ✨ 2. Key Features

### 🎮 3D Direct Station Management
- **Physical Interaction:** 청소, 수리, 발권, 보안 업무를 QTE(Quick Time Event) 및 물리적 상호작용으로 수행합니다.
- **Real-time Response:** 바닥의 오염, 설비 고장 등에 실시간으로 대응해야 합니다.

### 👥 4-Player Co-op & Role Play
- **Role Division:** `청소(정화)`, `수리(유지)`, `발권(안식)`, `보안(봉인)의 4가지 역할(발권, 보안 추가 예정)이 유기적으로 연결됩니다.

### ⚖️ 4 Metrics & Economy
- **Evaluation System:** 하루(10분)가 끝나면 완성도에 따라 평판 지수가 계산(+1/-1)됩니다. 
- **Shared Resources:** '레테(Lete)'와 '기억 파편(Memory Scrap)'을 모아 역 설비를 업그레이드하고 새로운 구역을 해금합니다.

### 🌐 Metaverse-like Shared Hub
- 단순한 게임 스테이지가 아닌, 플레이어들이 상주하고 소통하는 **'직장 메타버스'**입니다.
- 로비와 대합실은 소셜 허브 기능을 하며, 향후 이모트/제스처 등 커뮤니케이션 기능이 확장될 예정입니다.

---

## 🛠 3. Tech Stack & Architecture

### 3.1 Technology Stack

| Category | Technology |
| :--- | :--- |
| **Engine** | Unreal Engine 5 (C++ & Blueprint) |
| **Networking** | Unreal Replication system, UDP (IpNetDriver) |
| **Backend / Infra** | **PlayFab Multiplayer Servers (MPS)**, PlayFab CloudScript |
| **Orchestration** | **Azure Functions** (Node.js, HTTP Trigger) |
| **VCS** | Git (GitHub) |

### 3.2 Network Architecture (Session Orchestration)

이 프로젝트는 **UDP 기반 Dedicated Server**를 사용하며, 서버의 할당과 세션 관리를 위해 **PlayFab MPS Automation**과 **Azure Functions**를 활용한 오케스트레이션 레이어를 구축했습니다.
- **Dedicated Server:** 게임 규칙, AI, 월드 상태에 대한 권한(Authority)을 가짐
- **PlayFab MPS Automation:** 트래픽에 따라 서버 인스턴스를 자동으로 생성/회수
- **Azure Function:** PlayFab과 MPS 사이에서 로직을 중재하며, 향후 매치메이킹 로직 확장을 위한 오케스트레이션 레이어 역할 수행

## 🗺 4. Core Gameplay Loop
1 Day Cycle (10 Minutes)

1. 준비 (Preparation): 레테 여신상 앞에서 장비 점검 및 역할 분담
2. 업무 (Work Phase):
  - 🧹 청소: 오염 제거 및 기억 파편 회수
  - 🔧 수리: 고장난 설비 복구 (QTE)
3. 평가 (Evaluation): 업무 완료에 따라 평판 지수 증가/감소

## 👨‍💻 5. My Contributions
Role: Unreal Engine Client & Server Developer

My Role: Gameplay Programmer & Backend Engineer

Main Contributions
Gameplay Systems:

- Blueprint to C++ Refactoring:
  - 프로토타입 단계의 블루프린트 로직을 C++로 이식하여 성능 최적화 및 유지보수성 확보.
- Dedicated Server Logic:
  - Unreal Replication 및 RPC를 활용하여 멀티플레이어 환경에서의 청소/수리 상호작용 동기화 구현.
  - Server Authority 기반의 게임 상태 관리.
- Backend Integration:
  - PlayFab MPS 연동을 위한 Azure Functions 오케스트레이션 로직 구현.
  - 클라이언트 접속 흐름(Login -> Request Session -> Join) 설계.

## 📅 6. Roadmap & Future Work
- [ ] Advanced Social Hub (Emotes, Chat)
- [ ] Avatar Customization System
- [ ] Backend Migration (Planned)
  - Current: Azure Functions + CloudScript (Serverless)
  - Future: Spring Boot + RDS (DB) 도입
  - Steam 연동 및 자체 인증 시스템
  - WebSocket 기반 비공개 로비(방 코드) 시스템
  - 백엔드에서 직접 MPS Automation 제어 및 세션 로그 DB 영구 저장

---
This project was developed intensively over 5 weeks and is currently available as a Demo on Steam and Stove.
