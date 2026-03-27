# Parking Master WebASM

`주차의 달인` 감성에서 출발한 3D 모바일 웹 주차 게임입니다. 코어 로직은 `C++`로 작성했고, `raylib + Emscripten`으로 `WebAssembly` 번들을 만들어 브라우저에서 실행합니다.

## Links

- Live site: [Parking Master WebASM](https://parking-master-webasm.vercel.app)
- GitHub: [minwoo19930301/parking-master-webasm](https://github.com/minwoo19930301/parking-master-webasm)

## Features

- 1인칭 / 3인칭 카메라 전환
- 모바일 터치 버튼과 키보드 입력 동시 지원
- 좁은 베이 주차 + 평행 주차 챌린지
- C++ 단일 코드베이스에서 web build 생성
- 정적 파일만으로 배포 가능해서 Vercel에 바로 연결 가능

## Controls

- `WASD` or arrow keys: steer / accelerate / reverse
- `C`: toggle first-person / third-person
- `R`: retry current stage
- Mobile: on-screen steering, gas, reverse, camera, retry buttons

## Local Build

macOS 기준:

```bash
brew install cmake ninja emscripten
cd "/Users/minwokim/Documents/New project/parking-master-webasm"
npm run build:web
npm run preview
```

브라우저: `http://127.0.0.1:4173`

## Project Layout

- `src/main.cpp`: gameplay, cameras, touch UI, collision, parking validation
- `web/shell.html`: responsive fullscreen shell for wasm canvas
- `scripts/build-web.sh`: Emscripten build entrypoint
- `index.html`, `index.js`, `index.wasm`: generated web bundle committed for static hosting

## Vercel

이 저장소는 루트에 `index.html`이 생성되도록 구성했습니다. 그래서 별도 서버 없이도 Vercel에서 정적 사이트로 바로 배포할 수 있습니다.

- Production URL: [https://parking-master-webasm.vercel.app](https://parking-master-webasm.vercel.app)

1. GitHub public repo로 push
2. Vercel에서 해당 repo import
3. Framework preset 없이 deploy

## Stack

- `C++17`
- `raylib 5.5`
- `Emscripten 5`
- static hosting for Vercel / GitHub
