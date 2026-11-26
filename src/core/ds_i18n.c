// ds_i18n.c - UI string table: one key, eight languages, English fallback.
//
// The whole table comes from one X-macro list, so the enum, the key strings and
// the translations cannot drift apart. Each row is:
//   X(enum_name, "dotted.key", EN, VI, ZH, JA, KO, ES, FR, DE)
// The enum name cannot carry a dot, so nav.speech is spelled nav_speech there;
// the key the app passes to ds_i18n() is always the dotted string.
#include "ds_i18n.h"

#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

#define DS_I18N_KEYS(X) \
    X(nav_speech, "nav.speech", "Speech", "Lời nói", "语音", "音声", "음성", "Voz", "Synthèse", "Sprache") \
    X(nav_transcribe, "nav.transcribe", "Transcribe", "Chép lại", "转录", "書き起こす", "전사하기", "Transcribir", "Transcrire", "Transkribieren") \
    X(nav_mcp, "nav.mcp", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP") \
    X(nav_settings, "nav.settings", "Settings", "Cài đặt", "设置", "設定", "설정", "Ajustes", "Paramètres", "Einstellungen") \
    X(nav_session, "nav.session", "Session", "Phiên", "会话", "セッション", "세션", "Sesión", "Séance", "Sitzung") \
    X(tts_title, "tts.title", "Text to speech", "Văn bản thành lời nói", "文本转语音", "テキスト読み上げ", "텍스트 음성 변환", "Texto a voz", "Texte à la voix", "Text zu Sprache") \
    X(tts_subtitle, "tts.subtitle", "stella or tide, WAV or raw PCM. Markdown is filtered unless you turn it off.", "stella hoặc tide, WAV hoặc raw PCM. Markdown được lọc trừ khi bạn tắt tính năng này.", "stella 或 tide，WAV 或原始 PCM。Markdown 会被过滤，除非你将其关闭。", "stella または tide、WAV または raw PCM。Markdown は無効にしない限りフィルタリングされます。", "stella 또는 tide, WAV 또는 raw PCM. 마크다운은 비활성화하지 않으면 필터링됩니다.", "stella o tide, WAV o PCM sin comprimir. El Markdown se filtra a menos que lo desactives.", "stella ou tide, WAV ou PCM brut. Le Markdown est filtré sauf si vous l'avez désactivé.", "Stella oder Tide, WAV oder rohes PCM. Markdown wird gefiltert, es sei denn, du schaltest es aus.") \
    X(tts_voice, "tts.voice", "Voice", "Giọng nói", "语音", "声", "목소리", "Voz", "Voix", "Stimme") \
    X(tts_format, "tts.format", "Format", "Định dạng", "格式", "形式", "형식", "Formato", "Format", "Format") \
    X(tts_filter, "tts.filter", "Filter Markdown", "Lọc Markdown", "过滤 Markdown", "Markdownをフィルター", "마크다운 필터", "Filtrar Markdown", "Filtrer le Markdown", "Markdown filtern") \
    X(tts_preview, "tts.preview", "Preview", "Xem trước", "预览", "プレビュー", "미리보기", "Vista previa", "Aperçu", "Vorschau") \
    X(tts_generate, "tts.generate", "Generate", "Tạo", "生成", "生成", "생성", "Generar", "Générer", "Erzeugen") \
    X(tts_generating, "tts.generating", "Generating...", "Đang tạo...", "生成中...", "生成中...", "생성 중...", "Generando...", "Génération...", "Wird generiert...") \
    X(tts_clear, "tts.clear", "Clear", "Xóa", "清除", "クリア", "삭제", "Despejar", "Effacer", "Löschen") \
    X(tts_result, "tts.result", "Result", "Kết quả", "结果", "結果", "결과", "Resultado", "Résultat", "Ergebnis") \
    X(tts_no_audio, "tts.no_audio", "No audio yet", "Chưa có âm thanh", "暂无音频", "まだ音声なし", "아직 오디오 없음", "Aún sin audio", "Pas d'audio pour le moment", "Noch kein Audio") \
    X(tts_no_audio_desc, "tts.no_audio_desc", "Write text, pick a voice and press Generate.", "Viết văn bản, chọn giọng nói và nhấn Tạo.", "输入文字，选择语音，然后点击生成。", "テキストを入力し、声を選んで「生成」を押してください。", "텍스트를 입력하고, 음성을 선택한 뒤 생성을 누르세요.", "Escribe el texto, elige una voz y pulsa Generar.", "Écrivez du texte, choisissez une voix et appuyez sur Générer.", "Schreibe Text, wähle eine Stimme und drücke Erstellen.") \
    X(tts_history, "tts.history", "History", "Lịch sử", "历史", "歴史", "역사", "Historia", "Historique", "Geschichte") \
    X(tts_play, "tts.play", "Play", "Phát", "播放", "再生", "재생", "Reproducir", "Lire", "Abspielen") \
    X(tts_pause, "tts.pause", "Pause", "Tạm dừng", "暂停", "一時停止", "일시정지", "Pausar", "Mettre en pause", "Pausieren") \
    X(tts_open_folder, "tts.open_folder", "Open output folder", "Mở thư mục đầu ra", "打开输出文件夹", "出力フォルダを開く", "출력 폴더 열기", "Abrir carpeta de salida", "Ouvrir le dossier de sortie", "Ausgabepapierkorb öffnen") \
    X(tts_edited, "tts.edited", "edited", "đã chỉnh sửa", "已编辑", "編集済み", "편집됨", "editado", "édité", "bearbeitet") \
    X(tts_ready, "tts.ready", "ready", "sẵn sàng", "准备就绪", "準備完了", "준비됨", "listo", "prêt", "bereit") \
    X(tts_empty, "tts.empty", "empty", "trống", "空", "空", "빈", "vacío", "vide", "leer") \
    X(tts_over_limit, "tts.over_limit", "Over the character limit", "Vượt quá giới hạn ký tự", "超过字符限制", "文字数制限を超えています", "문자 수 제한 초과", "Superado el límite de caracteres", "Dépassement de la limite de caractères", "Über der Zeichenbegrenzung") \
    X(tts_over_limit_desc, "tts.over_limit_desc", "Shorten the text, or split it into several generations.", "Rút ngắn văn bản hoặc chia thành nhiều lần tạo.", "缩短文本，或分成几次生成。", "テキストを短くするか、複数回に分けて生成してください。", "텍스트를 줄이거나 여러 번 나눠 생성하세요.", "Acorta el texto o genéralo en varias partes.", "Raccourcissez le texte ou générez-le en plusieurs fois.", "Kürze den Text oder teile ihn in mehrere Schritte auf.") \
    X(asr_title, "asr.title", "Speech to text", "Chuyển lời nói thành văn bản", "语音转文字", "音声からテキストへ", "음성 텍스트 변환", "Discurso a texto", "Transcription de la parole", "Sprache in Text") \
    X(asr_subtitle, "asr.subtitle", "WAV or Ogg Opus in, the transcript out. Edit it in place and diff the correction against the automatic text.", "Nhập WAV hoặc Ogg Opus, xuất bản ghi chép. Chỉnh sửa tại chỗ và so sánh phần hiệu chỉnh với văn bản tự động.", "输入 WAV 或 Ogg Opus，输出转录文本。原地编辑，并将修正内容与自动文本进行差异比对。", "WAVまたはOgg Opusを入力、転写結果を出力。その場で編集し、修正内容を自動テキストと差分比較する。", "WAV 또는 Ogg Opus를 입력하면 전사본이 출력됩니다. 해당 위치에서 편집하고, 수정 내용을 자동 텍스트와 비교하세요.", "Entrada en WAV u Ogg Opus, salida en transcripción. Edítala en el mismo lugar y compara la corrección con el texto automático.", "Fichier WAV ou Ogg Opus en entrée, transcription en sortie. Editez-la sur place et comparez les corrections avec le texte automatique.", "WAV oder Ogg Opus rein, die Transkription raus. Bearbeite sie vor Ort und vergleiche die Korrektur mit dem automatischen Text.") \
    X(asr_audio, "asr.audio", "Audio", "Âm thanh", "音频", "オーディオ", "오디오", "Audio", "Audio", "Audio") \
    X(asr_drop_title, "asr.drop_title", "Drop a WAV or Ogg Opus file here", "Đặt tệp WAV hoặc Ogg Opus vào đây", "在此处放入 WAV 或 Ogg Opus 文件", "WAVまたはOgg Opusファイルをここにドロップしてください", "여기에 WAV 또는 Ogg Opus 파일을 드롭하세요", "Deja un archivo WAV u Ogg Opus aquí", "Déposez un fichier WAV ou Ogg Opus ici", "Lade eine WAV- oder Ogg-Opus-Datei hier hoch") \
    X(asr_drop_desc, "asr.drop_desc", "WAV, Ogg Opus, MP3 or FLAC, mono or stereo, up to 8 MiB and 120 s", "WAV, Ogg Opus, MP3 hoặc FLAC, đơn âm hoặc stereo, tối đa 8 MiB và 120 giây", "WAV、Ogg Opus、MP3 或 FLAC，单声道或立体声，最大 8 MiB 且最长 120 秒", "WAV、Ogg Opus、MP3、またはFLAC、モノまたはステレオ、最大8MiBおよび120秒", "WAV, Ogg Opus, MP3 또는 FLAC, 모노 또는 스테레오, 최대 8MiB 및 120초", "WAV, Ogg Opus, MP3 o FLAC, mono o estéreo, hasta 8 MiB y 120 s", "WAV, Ogg Opus, MP3 ou FLAC, mono ou stéréo, jusqu'à 8 Mo et 120 s", "WAV, Ogg Opus, MP3 oder FLAC, Mono oder Stereo, bis zu 8 MiB und 120 Sek.") \
    X(asr_browse, "asr.browse", "Browse...", "Duyệt...", "浏览...", "閲覧...", "찾아보기...", "Examinar...", "Parcourir...", "Durchsuchen...") \
    X(asr_transcribe, "asr.transcribe", "Transcribe", "Chép lại", "转录", "書き起こす", "전사하기", "Transcribir", "Transcrire", "Transkribieren") \
    X(asr_transcript, "asr.transcript", "Transcript", "Bản ghi lời", "文字记录", "文字起こし", "전사본", "Transcripción", "Transcription", "Transkript") \
    X(asr_no_transcript, "asr.no_transcript", "No transcript yet", "Chưa có bản ghi chép", "暂无文字记录", "まだ文字起こしがありません", "아직 대사록 없음", "Aún no hay transcripción", "Aucune transcription pour le moment", "Noch keine Transkription") \
    X(asr_no_transcript_desc, "asr.no_transcript_desc", "Transcribe a file, then correct the text right here.", "Chép nội dung tệp, sau đó hiệu đính văn bản ngay tại đây.", "转录文件，然后就在这里校对文本。", "ファイルを文字起こしして、その後ここでテキストを校正してください。", "파일을 전사한 뒤 바로 여기에서 텍스트를 교정하세요.", "Transcribe un archivo y luego corrige el texto justo aquí.", "Transcris un fichier, puis corrige le texte ici même.", "Transkribiere eine Datei und korrigiere den Text direkt hier.") \
    X(asr_lang_note, "asr.lang_note", "Language is detected automatically", "Ngôn ngữ được phát hiện tự động", "语言会自动检测", "言語は自動で検出されます", "언어가 자동으로 감지됩니다", "El idioma se detecta automáticamente", "La langue est détectée automatiquement", "Sprache wird automatisch erkannt") \
    X(asr_lang_desc, "asr.lang_desc", "On a very short clip the detector can pick the wrong language. A longer utterance is far more reliable.", "Trong một đoạn rất ngắn, bộ phát hiện có thể nhận nhầm ngôn ngữ. Một lời nói dài hơn sẽ đáng tin cậy hơn nhiều.", "在极短的片段中，检测器可能会误判语言。较长的语音则可靠得多。", "非常に短いクリップでは、検出器が誤った言語を判別してしまうことがあります。より長い発話のほうがはるかに信頼性が高くなります。", "아주 짧은 클립에서는 탐지기가 잘못된 언어를 감지할 수 있습니다. 더 긴 발화일수록 훨씬 더 신뢰할 수 있습니다.", "En un fragmento muy breve, el detector puede elegir el idioma equivocado. Una enunciación más larga es mucho más fiable.", "Sur une très courte séquence, le détecteur peut identifier le mauvais langage. Une énonciation plus longue est bien plus fiable.", "Bei einem sehr kurzen Ausschnitt kann der Detektor die falsche Sprache erkennen. Eine längere Äußerung ist weitaus zuverlässiger.") \
    X(asr_save, "asr.save", "Save transcript...", "Lưu bản ghi chép...", "保存记录…", "文字起こしを保存...", "전사 내용 저장...", "Guardar transcripción...", "Enregistrer la transcription...", "Transkript speichern ...") \
    X(asr_copy, "asr.copy", "Copy transcript", "Sao chép bản ghi", "复制转录文本", "文字起こしをコピー", "전사 내용 복사", "Copiar transcripción", "Copier la transcription", "Transkript kopieren") \
    X(asr_revert, "asr.revert", "Revert to automatic", "Quay lại tự động", "恢复为自动", "自動に戻す", "자동으로 복원", "Volver a automático", "Retourner à l'automatique", "Zurück auf Automatik") \
    X(asr_correction, "asr.correction", "Correction", "Sửa chữa", "更正", "訂正", "수정", "Corrección", "Correction", "Korrektur") \
    X(asr_loaded, "asr.loaded", "loaded", "đã tải", "已加载", "ロード済み", "로드됨", "cargado", "chargé", "geladen") \
    X(asr_automatic, "asr.automatic", "automatic", "tự động", "自动", "自動", "자동", "automático", "automatique", "automatisch") \
    X(mcp_title, "mcp.title", "MCP server", "Máy chủ MCP", "MCP服务器", "MCPサーバー", "MCP 서버", "Servidor MCP", "Serveur MCP", "MCP-Server") \
    X(mcp_subtitle, "mcp.subtitle", "The same core, exposed over the Model Context Protocol on stdio. An agent calls tts_synthesize or asr_transcribe and gets the result back.", "Cùng một lõi, được phơi ra qua Model Context Protocol trên stdio. Một agent gọi tts_synthesize hoặc asr_transcribe và nhận kết quả trở lại.", "同一个核心，在 stdio 上通过模型上下文协议暴露出来。智能体调用 tts_synthesize 或 asr_transcribe，随后获取返回结果。", "同じコアで、stdio 上のモデルコンテキストプロトコルを通じて露出している。エージェントが tts_synthesize または asr_transcribe を呼び出し、結果を受け取る。", "동일한 코어로, stdio 위의 모델 컨텍스트 프로토콜을 통해 노출됩니다. 에이전트가 tts_synthesize 또는 asr_transcribe를 호출하면 결과를 받아옵니다.", "El mismo núcleo, expuesto sobre el Protocolo de Contexto del Modelo en stdio. Un agente llama a tts_synthesize o asr_transcribe y recibe el resultado de vuelta.", "Le même noyau, exposé via le Model Context Protocol sur stdio. Un agent appelle tts_synthesize ou asr_transcribe et reçoit le résultat en retour.", "Derselbe Kern, über das Model Context Protocol auf stdio freigelegt. Ein Agent ruft tts_synthesize oder asr_transcribe auf und erhält das Ergebnis zurück.") \
    X(mcp_tools, "mcp.tools", "Tools", "Công cụ", "工具", "ツール", "도구", "Herramientas", "Outils", "Werkzeuge") \
    X(mcp_install, "mcp.install", "Install on this machine", "Cài đặt trên máy này", "安装到此机器上", "このマシンにインストール", "이 기계에 설치", "Instalar en esta máquina", "Installer sur cette machine", "Auf dieser Maschine installieren") \
    X(mcp_install_desc, "mcp.install_desc", "The binary is self-contained: C, simdjson and libcurl only. No Python, no Node, no runtime to install.", "Binary này tự đủ: chỉ gồm C, simdjson và libcurl. Không có Python, không có Node, không cần cài đặt môi trường chạy.", "二进制文件自包含：仅含 C、simdjson 和 libcurl。无需 Python，无需 Node，无需安装运行时。", "このバイナリは独立しており、C、simdjson、libcurlのみです。PythonもNodeもなく、インストールするランタイムもありません。", "이 바이너리는 독립적입니다: C, simdjson, libcurl만 포함됩니다. Python도, Node도, 설치할 런타임도 없습니다.", "El binario es autónomo: solo C, simdjson y libcurl. Sin Python, sin Node, sin entorno de ejecución que instalar.", "Le binaire est autonome : uniquement C, simdjson et libcurl. Pas de Python, pas de Node, pas d’environnement d’exécution à installer.", "Das Binary ist selbstständig: nur C, simdjson und libcurl. Kein Python, kein Node, keine Laufzeitumgebung zum Installieren.") \
    X(mcp_client_config, "mcp.client_config", "client config", "cấu hình client", "客户端配置", "クライアント設定", "클라이언트 설정", "configuración del cliente", "configuration client", "Client-Konfiguration") \
    X(mcp_self_test, "mcp.self_test", "Self test", "Tự kiểm tra", "自检", "自己テスト", "자가 테스트", "Autoprueba", "Auto-test", "Selbsttest") \
    X(mcp_run, "mcp.run", "Run self test", "Chạy kiểm tra tự động", "运行自检", "自己テストを実行", "자가 테스트 실행", "Ejecutar prueba automática", "Exécuter le test automatique", "Selbsttest durchführen") \
    X(mcp_self_test_desc, "mcp.self_test_desc", "Runs the server binary with --selftest and shows its tools/list reply.", "Chạy tệp nhị phân máy chủ với --selftest và hiển thị các công cụ/câu trả lời danh sách của nó.", "使用 --selftest 运行服务器二进制文件，并显示其工具/列表响应。", "サーバーバイナリを--selftestで実行し、そのツール／リスト応答を表示します。", "서버 바이너리를 --selftest로 실행하고 해당 도구/목록 응답을 표시합니다.", "Ejecuta el binario del servidor con --selftest y muestra sus herramientas/respuesta de lista.", "Exécute le binaire du serveur avec --selftest et affiche sa réponse outils/liste.", "Führt die Server-Binärdatei mit --selftest aus und zeigt deren Tools-/Listenantwort an.") \
    X(settings_title, "settings.title", "Settings", "Thiết lập", "设置", "設定", "설정", "Ajustes", "Paramètres", "Einstellungen") \
    X(settings_key, "settings.key", "DeepSeek API key", "Khóa API DeepSeek", "DeepSeek API密钥", "DeepSeek APIキー", "DeepSeek API 키", "Clave de API de DeepSeek", "Clé API DeepSeek", "DeepSeek-API-Schlüssel") \
    X(settings_base, "settings.base", "Base URL", "URL gốc", "基础URL", "ベースURL", "기본 URL", "URL base", "URL de base", "Basis-URL") \
    X(settings_out, "settings.out", "Output folder", "Thư mục đầu ra", "输出文件夹", "出力フォルダ", "출력 폴더", "Carpeta de salida", "Dossier de sortie", "Ausgabepfad") \
    X(settings_save, "settings.save", "Save settings", "Lưu cài đặt", "保存设置", "設定を保存", "설정 저장", "Guardar ajustes", "Enregistrer les paramètres", "Einstellungen speichern") \
    X(settings_reload, "settings.reload", "Reload from disk", "Tải lại từ đĩa", "从磁盘重新加载", "ディスクから再読み込み", "디스크에서 재로드", "Recargar desde disco", "Recharger depuis le disque", "Vom Datenträger nachladen") \
    X(settings_check, "settings.check", "Check key", "Kiểm tra chìa khóa", "检查钥匙", "鍵を確認する", "열쇠 확인", "Comprobar llave", "Vérifier la clé", "Schlüssel prüfen") \
    X(settings_limits, "settings.limits", "Documented limits", "Giới hạn đã được ghi chép", "已记录的限值", "記録された制限", "문서화된 한도", "Límites documentados", "Limites documentées", "Dokumentierte Grenzen") \
    X(settings_defaults, "settings.defaults", "Defaults and limits", "Giá trị mặc định và giới hạn", "默认值与限制", "デフォルトと制限", "기본값 및 제한", "Valores predeterminados y límites", "Valeurs par défaut et limites", "Standardeinstellungen und Grenzen") \
    X(settings_language, "settings.language", "Interface language", "Ngôn ngữ giao diện", "界面语言", "インターフェース言語", "인터페이스 언어", "Idioma de la interfaz", "Langue de l'interface", "Schnittstellen-Sprache") \
    X(settings_theme, "settings.theme", "Theme", "Chủ đề", "主题", "テーマ", "테마", "Tema", "Thème", "Thema") \
    X(settings_key_hint, "settings.key_hint", "Stored in the OS keychain when it is available. Never printed.", "Được lưu trong kho khóa của hệ điều hành khi có. Không bao giờ hiển thị.", "在 OS 密钥串中存储（当可用时）。绝不打印。", "利用可能な場合はOSキーチェーンに保存される。印刷されることはない。", "사용 가능할 경우 OS 키체인에 저장됩니다. 절대 인쇄되지 않습니다.", "Almacenado en el llavero del SO cuando está disponible. Nunca se imprime.", "Stocké dans le portefeuille de clés du système d'exploitation lorsqu'il est disponible. Jamais imprimé.", "Wird im OS-Schlüsselbund gespeichert, wenn er verfügbar ist. Niemals ausgedruckt.") \
    X(settings_in_force, "settings.in_force", "In force now", "Đang có hiệu lực", "现已生效", "現在、全力で", "현재 전력 투입 중", "En vigor ahora", "En action maintenant", "Jetzt mit voller Kraft") \
    X(settings_cjk_note, "settings.cjk_note", "A CJK character counts as two billed characters. A response that ends early is treated as a failure, not as short audio.", "Một ký tự CJK được tính là hai ký tự trong bảng tính phí. Phản hồi kết thúc sớm sẽ được coi là thất bại, chứ không phải âm thanh ngắn.", "一个CJK字符计为两个计费字符。提前结束的回复被视为失败，而非短音频。", "CJK文字は2文字として計上されます。早期に終了する応答は、短い音声ではなく失敗とみなされます。", "한자는 두 개의 과금 문자로 산정됩니다. 조기에 종료된 응답은 짧은 오디오가 아니라 실패로 처리됩니다.", "Un carácter chino, japonés o coreano se cuenta como dos caracteres facturados. Una respuesta que finaliza antes de tiempo se considera un fallo, no audio corto.", "Un caractère CJK compte pour deux caractères facturés. Une réponse qui se termine prématurément est considérée comme un échec, et non comme un message audio court.", "Ein CJK-Zeichen zählt als zwei abgerechnete Zeichen. Eine vorzeitig endende Antwort gilt als Fehler und nicht als kurzes Audio.") \
    X(session_last_job, "session.last_job", "Last job", "Nhiệm vụ trước", "上一份工作", "最後の仕事", "마지막 임무", "Último trabajo", "Dernier travail", "Letzter Auftrag") \
    X(session_none, "session.none", "No job yet. The last TTS or ASR result is summarised here.", "Chưa có công việc nào. Kết quả TTS hoặc ASR gần nhất được tóm tắt ở đây.", "暂无任务。此处汇总了最新的TTS或ASR结果。", "まだジョブはありません。最後のTTSまたはASR結果がここに要約されています。", "아직 작업이 없습니다. 마지막 TTS 또는 ASR 결과가 여기에 요약되어 있습니다.", "Aún no hay tarea. El último resultado de TTS o ASR se resume aquí.", "Aucun travail pour le moment. Le dernier résultat de synthèse vocale ou de reconnaissance vocale est résumé ici.", "Noch kein Auftrag. Das letzte TTS- oder ASR-Ergebnis wird hier zusammengefasst.") \
    X(session_activity, "session.activity", "Activity", "Hoạt động", "活动", "活動", "활동", "Actividad", "Activité", "Aktivität") \
    X(session_activity_none, "session.activity_none", "Nothing logged yet.", "Chưa có gì được ghi lại.", "尚未记录任何内容。", "まだ記録なし。", "아직 기록된 내용이 없습니다.", "Aún no hay registros.", "Rien n'a encore été enregistré.", "Noch nichts protokolliert.") \
    X(session_clear, "session.clear", "Clear history", "Xóa lịch sử", "清除历史记录", "履歴をクリア", "기록 지우기", "Borrar historial", "Effacer l'historique", "Verlauf löschen") \
    X(session_continuous_note, "session.continuous_note", "This window renders continuously so the waveform and progress stay live, which uses CPU while idle.", "Cửa sổ này được vẽ liên tục để sóng hình và tiến độ luôn cập nhật theo thời gian thực, việc này sẽ tiêu tốn CPU khi không có tác vụ nào khác.", "此窗口持续渲染，以便波形和进度保持实时更新，空闲时会占用 CPU。", "このウィンドウは波形と進行状況をリアルタイムに表示するために継続的にレンダリングされ、待機時でもCPUを使用します。", "이 창은 파형과 진행 상황이 실시간으로 유지되도록 지속적으로 렌더링하므로, 대기 중에도 CPU를 사용합니다.", "Esta ventana se renderiza continuamente para que la forma de onda y el progreso permanezcan actualizados, lo que consume CPU cuando no hay actividad.", "Cette fenêtre s'affiche en continu afin que la forme d'onde et la progression restent à jour, ce qui consomme de la puissance de traitement CPU lorsque l'application est inutilisée.", "Dieses Fenster wird kontinuierlich gerendert, damit Wellenform und Fortschritt aktuell bleiben; dabei wird im Leerlauf CPU genutzt.") \
    X(common_edited, "common.edited", "edited", "đã chỉnh sửa", "已编辑", "編集済み", "편집됨", "editado", "édité", "bearbeitet") \
    X(common_failed, "common.failed", "failed", "thất bại", "失败了", "失敗", "실패", "falló", "a échoué", "fehlgeschlagen") \
    X(common_ok, "common.ok", "ok", "được", "好的", "オーケー", "좋아", "Vale", "d'accord", "ok") \
    X(common_browse, "common.browse", "Browse...", "Duyệt...", "浏览...", "ブラウズ…", "찾아보기...", "Explorar...", "Parcourir...", "Durchsuchen...") \
    X(common_edit_too_long, "common.edit_too_long", "That edit did not fit the buffer", "Bản chỉnh sửa đó không vừa với bộ đệm", "该编辑不符合缓冲区", "その編集はバッファに収まりませんでした", "그 편집은 버퍼에 맞지 않았습니다", "Esa edición no encajó en el búfer.", "Ce montage ne correspondait pas au tampon", "Dieser Schnitt passte nicht in den Puffer") \
    X(tts_fail_title, "tts.fail_title", "Synthesis failed", "Tạo giọng nói thất bại", "合成失败", "合成に失敗しました", "합성에 실패했습니다", "La síntesis ha fallado", "Échec de la synthèse", "Synthese fehlgeschlagen") \
    X(tts_clip, "tts.clip", "Generated clip", "Đoạn clip đã tạo", "生成的片段", "生成されたクリップ", "생성된 클립", "Clip generado", "Clip généré", "Erzeugter Clip") \
    X(tts_open_folder_fail, "tts.open_folder_fail", "Could not open the output folder", "Không thể mở thư mục đầu ra", "无法打开输出文件夹", "出力フォルダを開けませんでした", "출력 폴더를 열 수 없습니다", "No se ha podido abrir la carpeta de salida", "Impossible d'ouvrir le dossier de sortie", "Ausgabepfad konnte nicht geöffnet werden") \
    X(tts_clear_cache, "tts.clear_cache", "Clear chunk cache", "Xóa bộ nhớ đệm khối", "清除块缓存", "チャンクキャッシュをクリア", "큐브 캐시 지우기", "Borrar caché de bloques", "Vider le cache des blocs", "Chunk-Cache leeren") \
    X(tts_cache_cleared, "tts.cache_cleared", "chunk cache cleared", "bộ nhớ đệm chunk đã được xóa", "块缓存已清除", "チャンクキャッシュがクリアされました", "청크 캐시 삭제됨", "caché de trozos eliminado", "cache de blocs vidé", "Chunk-Cache geleert") \
    X(tts_history_none, "tts.history_none", "Nothing yet. A generation or a transcription lands here.", "Chưa có gì. Kết quả tạo hoặc nhận dạng sẽ hiện ở đây.", "还没有内容。生成或转录的结果会显示在这里。", "まだ何もありません。生成や文字起こしの結果がここに表示されます。", "아직 없습니다. 생성 또는 전사 결과가 여기에 표시됩니다.", "Nada por ahora. Una generación o una transcripción llega aquí.", "Rien pour l'instant. Une génération ou une transcription arrive ici.", "Noch nichts. Das Ergebnis einer Synthese oder Transkription erscheint hier.") \
    X(tts_placeholder, "tts.placeholder", "Write the text to read...", "Viết văn bản cần đọc...", "输入要朗读的文本...", "読むテキストを入力してください...", "읽을 텍스트를 작성하세요...", "Escribe el texto que se leerá...", "Écrivez le texte à lire...", "Text zum Vorlesen schreiben ...") \
    X(tts_col_time, "tts.col_time", "Time", "Thời gian", "时间", "時間", "시간", "Tiempo", "Temps", "Zeit") \
    X(tts_col_kind, "tts.col_kind", "Kind", "Loại", "类型", "種類", "유형", "Tipo", "Type", "Art") \
    X(tts_col_detail, "tts.col_detail", "Detail", "Chi tiết", "详情", "詳細", "세부 사항", "Detalle", "Détail", "Details") \
    X(tts_col_size, "tts.col_size", "Size", "Kích thước", "尺寸", "サイズ", "크기", "Tamaño", "Taille", "Größe") \
    X(tts_col_text, "tts.col_text", "Text", "Văn bản", "文本", "テキスト", "텍스트", "Texto", "Texte", "Text") \
    X(tts_col_status, "tts.col_status", "Status", "Trạng thái", "状态", "ステータス", "상태", "Estado", "Statut", "Status") \
    X(asr_choose, "asr.choose", "Choose audio to transcribe", "Chọn âm thanh để chuyển thành văn bản", "选择要转录的音频", "音声を選択して文字起こしする", "전사할 오디오를 선택하세요", "Elige el audio que quieres transcribir", "Choisir l'audio à transcrire", "Audio zum Transkribieren auswählen") \
    X(asr_fail_title, "asr.fail_title", "Transcription failed", "Phiên âm thất bại", "转录失败", "転写に失敗しました", "전사 실패", "Error en la transcripción", "Échec de la transcription", "Transkription fehlgeschlagen") \
    X(asr_convert, "asr.convert", "MP3 and FLAC are converted to WAV first", "MP3 và FLAC được chuyển đổi sang WAV trước", "MP3和FLAC首先转换为WAV", "MP3とFLACはまずWAVに変換されます", "MP3와 FLAC은 먼저 WAV로 변환됩니다", "Los archivos MP3 y FLAC se convierten primero a WAV", "Les fichiers MP3 et FLAC sont d'abord convertis en WAV", "MP3 und FLAC werden zuerst in WAV konvertiert") \
    X(asr_convert_desc, "asr.convert_desc", "ASR takes WAV or Ogg Opus. Anything else is decoded locally, then uploaded as WAV.", "ASR nhận WAV hoặc Ogg Opus. Mọi định dạng khác sẽ được giải mã tại chỗ, sau đó tải lên dưới dạng WAV.", "ASR 接受 WAV 或 Ogg Opus。其他任何格式均在本地解码，然后作为 WAV 上传。", "ASRはWAVまたはOgg Opusを受け付けます。それ以外のものはローカルでデコードされ、その後WAVとしてアップロードされます。", "ASR은 WAV 또는 Ogg Opus를 사용합니다. 그 외의 모든 것은 로컬에서 디코딩된 후 WAV로 업로드됩니다.", "ASR acepta WAV u Ogg Opus. Todo lo demás se decodifica localmente y luego se sube como WAV.", "ASR prend en charge WAV ou Ogg Opus. Tout le reste est décodé localement, puis téléversé au format WAV.", "ASR akzeptiert WAV oder Ogg Opus. Alles andere wird lokal decodiert und anschließend als WAV hochgeladen.") \
    X(asr_transcribing, "asr.transcribing", "Transcribing...", "Đang chép lời...", "正在转录...", "文字起こし中...", "전사 중...", "Transcribiendo...", "Transcription en cours...", "Transkribieren...") \
    X(asr_corrected, "asr.corrected", "corrected", "đã sửa", "已更正", "修正済み", "수정됨", "corregido", "corrigé", "korrigiert") \
    X(asr_placeholder, "asr.placeholder", "The transcript appears here...", "Bản ghi chép xuất hiện ở đây...", "文字记录显示在此处……", "文字起こしがここに表示されます...", "전사본이 여기에 표시됩니다...", "La transcripción aparece aquí...", "La transcription apparaît ici...", "Die Transkription erscheint hier ...") \
    X(asr_save_title, "asr.save_title", "Save transcript", "Lưu bản ghi", "保存转录内容", "文字起こしを保存", "전사 내용 저장", "Guardar transcripción", "Enregistrer la transcription", "Transkript speichern") \
    X(asr_saved, "asr.saved", "saved %s", "đã lưu %s", "已保存 %s", "%sをセーブしました", "%s 저장됨", "se ha guardado %s", "a sauvé %s", "%s gespeichert") \
    X(asr_save_failed, "asr_save_failed", "cannot write %s", "không thể ghi %s", "无法写入 %s", "%sを書き込めません", "%s를 쓸 수 없습니다", "no se puede escribir %s", "impossible d'écrire %s", "kann %s nicht schreiben") \
    X(mcp_not_run, "mcp.not_run", "not run yet", "chưa chạy", "尚未运行", "まだ実行しない", "아직 실행하지 않음", "todavía no correr", "pas encore courir", "noch nicht laufen") \
    X(mcp_selftest_run, "mcp.selftest_run", "selftest run", "chạy tự kiểm tra", "自检运行", "セルフテスト実行", "자가 테스트 실행", "ejecución de autoprueba", "exécution de test auto", "Selbsttestlauf") \
    X(mcp_run_again, "mcp.run_again", "Run again", "Chạy lại", "再次运行", "もう一度実行", "다시 실행", "Ejecutar de nuevo", "Relancer", "Erneut laufen") \
    X(mcp_tool_tts, "mcp.tool_tts", "Text to audio. Writes the file, returns the audio and its metadata.", "Chuyển văn bản thành âm thanh. Viết tệp, trả về âm thanh và siêu dữ liệu của nó.", "文本转语音。写入文件，返回音频及其元数据。", "テキストから音声へ。ファイルを書き出し、音声とそのメタデータを返す。", "텍스트를 오디오로 변환합니다. 파일을 작성하고, 오디오와 그 메타데이터를 반환합니다.", "Texto a audio. Escribe el archivo, devuelve el audio y sus metadatos.", "Texte en audio. Écrit le fichier, renvoie l'audio et ses métadonnées.", "Text zu Audio. Schreibt die Datei, gibt das Audio und seine Metadaten zurück.") \
    X(mcp_tool_asr, "mcp.tool_asr", "Audio to text. WAV or Ogg Opus, language detected.", "Âm thanh sang văn bản. WAV hoặc Ogg Opus, ngôn ngữ đã được nhận diện.", "语音转文本。WAV 或 Ogg Opus，语言已检测。", "音声からテキスト。WAVまたはOgg Opus、言語を検出。", "음성 텍스트 변환. WAV 또는 Ogg Opus, 언어 감지됨.", "Audio a texto. WAV u Ogg Opus, idioma detectado.", "Audio vers texte. WAV ou Ogg Opus, langue détectée.", "Audio zu Text. WAV oder Ogg Opus, Sprache erkannt.") \
    X(mcp_tool_config, "mcp.tool_config", "Base URL, whether a key is set, output folder.", "URL gốc, xem một khóa có được thiết lập hay không, thư mục đầu ra.", "基础URL、是否设置了密钥、输出文件夹。", "ベースURL、キーが設定されているかどうか、出力フォルダ。", "기본 URL, 키가 설정되었는지 여부, 출력 폴더.", "URL base, si se ha establecido una clave, carpeta de salida.", "URL de base, indique si une clé est définie, dossier de sortie.", "Basis-URL, ob eine Schlüssel festgelegt ist, Ausgabepfad.") \
    X(mcp_tool_health, "mcp.tool_health", "Liveness and version.", "Tính sống động và phiên bản.", "活跃度与版本。", "ライブネスとバージョン。", "활성 상태와 버전.", "Vitalidad y versión.", "Vigueur et version.", "Lebendigkeit und Version.") \
    X(mcp_transport, "mcp.transport", "stdio transport, JSON-RPC 2.0. One request per line, one response per line.", "Giao thức truyền dữ liệu stdio, JSON-RPC 2.0. Một yêu cầu mỗi dòng, một phản hồi mỗi dòng.", "stdio 传输，JSON-RPC 2.0。每行一个请求，每行一个响应。", "stdioトランスポート、JSON-RPC 2.0。1行につき1リクエスト、1行につき1レスポンス。", "stdio 전송, JSON-RPC 2.0. 한 줄에 하나의 요청, 한 줄에 하나의 응답.", "Transporte de stdio, JSON-RPC 2.0. Una solicitud por línea, una respuesta por línea.", "Transport stdio, JSON-RPC 2.0. Une requête par ligne, une réponse par ligne.", "stdio-Transport, JSON-RPC 2.0. Eine Anfrage pro Zeile, eine Antwort pro Zeile.") \
    X(mcp_install_comment, "mcp.install_comment", "# copy the server next to this app into ~/.local/bin", "# sao chép máy chủ bên cạnh ứng dụng này vào ~/.local/bin", "# 将此应用旁边的服务器复制到 ~/.local/bin", "# このアプリの隣にあるサーバーを ~/.local/bin にコピー", "# 이 앱 옆에 있는 서버를 ~/.local/bin에 복사하세요", "# copia el servidor junto a esta aplicación en ~/.local/bin", "# copier le serveur à côté de cette application dans ~/.local/bin", "# Kopiere den Server neben dieser App in ~/.local/bin") \
    X(settings_key_source, "settings.key_source", "The key is read from $DEEPSEEK_API_KEY, then this app's own config file, then the DeepSeek provider block in kimi-code's config.", "Chìa khóa được đọc từ $DEEPSEEK_API_KEY, sau đó là tệp cấu hình riêng của ứng dụng này, rồi đến khối nhà cung cấp DeepSeek trong cấu hình của kimi-code.", "密钥依次从 $DEEPSEEK_API_KEY、本应用自身的配置文件，以及 kimi-code 配置中的 DeepSeek 提供商块中读取。", "キーは$DEEPSEEK_API_KEYから読み取られ、次にこのアプリ自身の設定ファイル、その後にkimi-codeの設定内のDeepSeekプロバイダーブロックから読み取られる。", "키는 $DEEPSEEK_API_KEY에서 읽고, 그다음 이 앱 자체의 설정 파일에서 읽으며, 그다음 kimi-code 설정의 DeepSeek 제공자 블록에서 읽습니다.", "La clave se lee desde $DEEPSEEK_API_KEY, luego del archivo de configuración propio de esta aplicación y, finalmente, del bloque del proveedor DeepSeek en la configuración de kimi-code.", "La clé est lue dans $DEEPSEEK_API_KEY, puis dans le fichier de configuration propre à cette application, puis dans le bloc fournisseur DeepSeek de la configuration de kimi-code.", "Der Schlüssel wird aus $DEEPSEEK_API_KEY gelesen, anschließend aus der eigenen Konfigurationsdatei dieser App und dann aus dem DeepSeek-Anbieterblock in der Konfiguration von kimi-code.") \
    X(settings_checking, "settings.checking", "Checking...", "Đang kiểm tra...", "检查中...", "確認中...", "확인 중...", "Comprobando...", "Vérification en cours...", "Wird überprüft …") \
    X(settings_choose_out, "settings.choose_out", "Choose the output folder", "Chọn thư mục đầu ra", "选择输出文件夹", "出力フォルダを選択", "출력 폴더를 선택하세요", "Elige la carpeta de salida", "Choisissez le dossier de sortie", "Ausgabepfad auswählen") \
    X(settings_saved, "settings.saved", "saved to %s", "đã lưu vào %s", "已保存至 %s", "%sに保存しました", "%s에 저장됨", "guardado en %s", "sauvegardé dans %s", "gespeichert unter %s") \
    X(settings_reloaded, "settings.reloaded", "reloaded", "tái nạp", "已重新装填", "リロード", "재장전됨", "recargado", "rechargé", "nachgeladen") \
    X(settings_filter_default, "settings.filter_default", "Filter Markdown by default", "Lọc Markdown theo mặc định", "默认按 Markdown 过滤", "デフォルトでMarkdownをフィルタリング", "기본적으로 마크다운 필터 적용", "Filtrar Markdown por defecto", "Filtrer le Markdown par défaut", "Markdown standardmäßig filtern") \
    X(settings_tts_timeout, "settings.tts_timeout", "TTS timeout", "Hết thời gian TTS", "TTS超时", "TTSタイムアウト", "TTS 시간 초과", "Tiempo de espera de TTS", "Délai d'attente de la synthèse vocale expiré", "TTS-Auszeit") \
    X(settings_asr_timeout, "settings.asr_timeout", "ASR timeout", "Hết thời gian chờ ASR", "ASR超时", "ASRタイムアウト", "ASR 타임아웃", "Tiempo de espera de ASR", "Délai d'attente ASR", "ASR-Timeout") \
    X(settings_history_limit, "settings.history_limit", "History entries", "Các mục lịch sử", "历史条目", "履歴エントリ", "역사 기록", "Entradas de historia", "Entrées historiques", "Historie-Einträge") \
    X(settings_waveform_bars, "settings.waveform_bars", "Waveform bars", "Thanh sóng hình", "波形条", "波形バー", "파형 막대", "Barras de forma de onda", "Barres de forme d'onde", "Wellenformleisten") \
    X(settings_lim_tts_text, "settings.lim_tts_text", "TTS TEXT", "VĂN BẢN TTS", "TTS 文本", "TTSテキスト", "TTS 텍스트", "TEXTO DE V.O.", "TEXTE PAR SYNTHÈSE VOCALE", "TTS-TEXT") \
    X(settings_lim_tts_body, "settings.lim_tts_body", "TTS BODY", "THÂN TTS", "TTS主体", "TTSボディ", "TTS 바디", "CUERPO DE TTS", "CORPS DU TTS", "TTS-KÖRPER") \
    X(settings_lim_tts_length, "settings.lim_tts_length", "TTS LENGTH", "ĐỘ DÀI TTS", "TTS 长度", "TTSの長さ", "TTS 길이", "LONGITUD DE Voz en Texto", "LONGUEUR DU TTS", "TTS-LÄNGE") \
    X(settings_lim_asr_file, "settings.lim_asr_file", "ASR FILE", "TỆP ASR", "ASR 文件", "ASRファイル", "ASR 파일", "ARCHIVO ASR", "FICHE ASR", "ASR-DATEI") \
    X(settings_lim_asr_length, "settings.lim_asr_length", "ASR LENGTH", "ĐỘ DÀI ASR", "ASR长度", "ASR長さ", "ASR 길이", "LONGITUD DE ASR", "LONGUEUR DE L'ASR", "ASL-LÄNGE") \
    X(settings_lim_asr_upload, "settings.lim_asr_upload", "ASR UPLOAD", "TẢI LÊN ASR", "ASR 上传", "ASRアップロード", "ASR 업로드", "SUBIDA DE ASR", "TÉLÉCHARGEMENT DE L'ASR", "ASR-UPLOAD")

typedef enum {
#define DS_KEY_ENUM(name, ...) DS_K_##name,
    DS_I18N_KEYS(DS_KEY_ENUM)
#undef DS_KEY_ENUM
    DS_K_COUNT
} ds_i18n_key;

static const struct ds_i18n_entry {
    const char *key;
    const char *v[DS_LANG_COUNT];
} kTable[] = {
#define DS_KEY_ROW(name, key, ...) { key, { __VA_ARGS__ } },
    DS_I18N_KEYS(DS_KEY_ROW)
#undef DS_KEY_ROW
};

/* The enum and the table are generated from the same list; this fails to
   compile if that ever stops being true. */
typedef char ds_i18n_table_check[
    (int)(sizeof(kTable) / sizeof(kTable[0])) == DS_K_COUNT ? 1 : -1];

static const char *const kCodes[DS_LANG_COUNT] = {
    "en", "vi", "zh", "ja", "ko", "es", "fr", "de"
};

static int g_lang = DS_LANG_EN;

const char *ds_language_code(int index) {
    if (index < 0 || index >= DS_LANG_COUNT) return NULL;
    return kCodes[index];
}

int ds_language_index(const char *code) {
    int i;
    if (!code) return -1;
    for (i = 0; i < DS_LANG_COUNT; i++) {
        if (strcmp(kCodes[i], code) == 0) return i;
    }
    return -1;
}

void ds_i18n_set(int lang) {
    if (lang < 0 || lang >= DS_LANG_COUNT) lang = DS_LANG_EN;
    g_lang = lang;
}

int ds_i18n_get(void) {
    return g_lang;
}

const char *ds_i18n(const char *key) {
    int i;
    int lang = g_lang;
    if (!key) return NULL;
    if (lang < 0 || lang >= DS_LANG_COUNT) lang = DS_LANG_EN;
    for (i = 0; i < (int)(sizeof(kTable) / sizeof(kTable[0])); i++) {
        if (strcmp(kTable[i].key, key) == 0) {
            const char *s = kTable[i].v[lang];
            if (!s || !*s) s = kTable[i].v[DS_LANG_EN];
            return s;
        }
    }
    return NULL;
}

/* Lower-case the primary subtag of a BCP 47 / POSIX locale tag and compare it
   against the eight supported codes. "zh-Hans-CN", "vi_VN.UTF-8" and "fr" all
   reduce to their primary subtag. */
static int ds_lang_from_code(const char *code) {
    char buf[16];
    size_t n = 0;
    if (!code) return DS_LANG_EN;
    while (code[n] != '\0' && code[n] != '-' && code[n] != '_' && code[n] != '.' &&
           n + 1 < sizeof(buf)) {
        char c = code[n];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        buf[n] = c;
        n++;
    }
    buf[n] = '\0';
    if (n == 0) return DS_LANG_EN;
    if (strcmp(buf, "vi") == 0) return DS_LANG_VI;
    if (strcmp(buf, "zh") == 0) return DS_LANG_ZH;
    if (strcmp(buf, "ja") == 0) return DS_LANG_JA;
    if (strcmp(buf, "ko") == 0) return DS_LANG_KO;
    if (strcmp(buf, "es") == 0) return DS_LANG_ES;
    if (strcmp(buf, "fr") == 0) return DS_LANG_FR;
    if (strcmp(buf, "de") == 0) return DS_LANG_DE;
    return DS_LANG_EN;
}

#if defined(__APPLE__)

int ds_language_detect(void) {
    CFArrayRef langs = CFLocaleCopyPreferredLanguages();
    char buf[64];
    buf[0] = '\0';
    if (langs && CFArrayGetCount(langs) > 0) {
        CFStringRef s = (CFStringRef)CFArrayGetValueAtIndex(langs, 0);
        if (s && !CFStringGetCString(s, buf, (CFIndex)sizeof(buf), kCFStringEncodingUTF8))
            buf[0] = '\0';
    }
    if (langs) CFRelease(langs);
    return ds_lang_from_code(buf);
}

#elif defined(_WIN32)

int ds_language_detect(void) {
    WCHAR wbuf[160];
    char buf[160];
    int n = GetUserDefaultLocaleName(wbuf, (int)(sizeof(wbuf) / sizeof(wbuf[0])));
    int len;
    if (n <= 0) return DS_LANG_EN;
    len = WideCharToMultiByte(CP_UTF8, 0, wbuf, n - 1, buf,
                              (int)sizeof(buf) - 1, NULL, NULL);
    if (len <= 0) return DS_LANG_EN;
    buf[len] = '\0';
    return ds_lang_from_code(buf);
}

#else

int ds_language_detect(void) {
    const char *s = getenv("LC_ALL");
    if (!s || !*s) s = getenv("LC_MESSAGES");
    if (!s || !*s) s = getenv("LANG");
    return ds_lang_from_code(s);
}

#endif
