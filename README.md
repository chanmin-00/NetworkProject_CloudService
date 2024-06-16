# NetworkProject_CloudService
2024학년도 1학기 네트워크 프로그래밍 수업에서 텀프로젝트로 진행했던 프로젝트 구현물입니다.

## 사용한 기술
C언어를 기반으로 하여 네트워크 프로그래밍을 위해 사용하였던 기술들에 대하여 설명합니다.
- 다중 사용자 연결 및 처리 및 실시간 채팅 송수신을 위한 멀티스레딩
- 공유 자원에 대한 동기화 처리를 위한 뮤텍스 및 세마포어
- 암호화된 통신을 위한 TLS/SSL

## 핵심 기능
구현된 결과물에 대하여 주요 기능을 설명합니다.
📄 파일 업로드 및 다운로드 기능
- 인증된 사용자 간 텍스트, 이미지 등의 다양한 파일 업로드 및 다운로드
- 서버 내 개별 사용자 디렉토리 생성 및 비밀번호를 통한 디렉토리 접근 인증

🛡️ TSL/SSL 기반 파일 전송자 간 인증 시스템
- 디지털 서명 및 인증을 통해 보안화
- 보안화된 데이터 전송

👨‍👩‍👧‍👦 파일 공유자 간 채팅 시스템
- 디렉토리(채팅방) 공유자간의 파일 작업 관련 실시간 채팅
