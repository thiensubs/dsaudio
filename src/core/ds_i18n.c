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
    X(nav_transcribe, "nav.transcribe", "Transcribe", "Phiên âm", "转录", "文字起こし", "전사", "Transcribir", "Transcrire", "Transkribieren") \
    X(nav_mcp, "nav.mcp", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP", "MCP") \
    X(nav_settings, "nav.settings", "Settings", "Cài đặt", "设置", "設定", "설정", "Ajustes", "Réglages", "Einstellungen") \
    X(nav_session, "nav.session", "Session", "Phiên", "会话", "セッション", "세션", "Sesión", "Session", "Sitzung") \
    X(tts_title, "tts.title", "Text to speech", "Chuyển văn bản thành giọng nói", "文本转语音", "テキスト読み上げ", "텍스트 음성 변환", "Texto a voz", "Synthèse vocale", "Text in Sprache") \
    X(tts_subtitle, "tts.subtitle", "stella or tide, WAV or raw PCM. Markdown is filtered unless you turn it off.", "stella hoặc tide, WAV hoặc PCM thô. Markdown được lọc trừ khi bạn tắt.", "stella 或 tide，WAV 或原始 PCM。除非关闭，否则 Markdown 会被过滤。", "stella または tide、WAV または生 PCM。Markdown はオフにしない限り除去されます。", "stella 또는 tide, WAV 또는 원시 PCM. 끄지 않으면 Markdown이 필터링됩니다.", "stella o tide, WAV o PCM sin procesar. El Markdown se filtra a menos que lo desactives.", "stella ou tide, WAV ou PCM brut. Le Markdown est filtré sauf si vous le désactivez.", "stella oder tide, WAV oder rohes PCM. Markdown wird gefiltert, es sei denn, du schaltest es ab.") \
    X(tts_voice, "tts.voice", "Voice", "Giọng đọc", "音色", "音声", "음성", "Voz", "Voix", "Stimme") \
    X(tts_format, "tts.format", "Format", "Định dạng", "格式", "形式", "형식", "Formato", "Format", "Format") \
    X(tts_filter, "tts.filter", "Filter Markdown", "Lọc Markdown", "过滤 Markdown", "Markdown を除去", "Markdown 필터링", "Filtrar Markdown", "Filtrer le Markdown", "Markdown filtern") \
    X(tts_preview, "tts.preview", "Preview", "Xem trước", "预览", "プレビュー", "미리보기", "Vista previa", "Aperçu", "Vorschau") \
    X(tts_generate, "tts.generate", "Generate", "Tạo", "生成", "生成", "생성", "Generar", "Générer", "Generieren") \
    X(tts_generating, "tts.generating", "Generating...", "Đang tạo...", "正在生成...", "生成中...", "생성 중...", "Generando...", "Génération...", "Wird generiert...") \
    X(tts_clear, "tts.clear", "Clear", "Xóa", "清除", "クリア", "지우기", "Borrar", "Effacer", "Leeren") \
    X(tts_result, "tts.result", "Result", "Kết quả", "结果", "結果", "결과", "Resultado", "Résultat", "Ergebnis") \
    X(tts_no_audio, "tts.no_audio", "No audio yet", "Chưa có âm thanh", "还没有音频", "まだ音声がありません", "아직 오디오가 없습니다", "Aún no hay audio", "Pas encore d'audio", "Noch kein Audio") \
    X(tts_no_audio_desc, "tts.no_audio_desc", "Write text, pick a voice and press Generate.", "Nhập văn bản, chọn giọng đọc rồi nhấn Tạo.", "输入文字，选择音色，然后点击生成。", "テキストを入力し、音声を選んで「生成」を押してください。", "텍스트를 입력하고 음성을 선택한 뒤 생성을 누르세요.", "Escribe el texto, elige una voz y pulsa Generar.", "Saisissez le texte, choisissez une voix puis cliquez sur Générer.", "Text schreiben, Stimme wählen und auf Generieren klicken.") \
    X(tts_history, "tts.history", "History", "Lịch sử", "历史记录", "履歴", "기록", "Historial", "Historique", "Verlauf") \
    X(tts_play, "tts.play", "Play", "Phát", "播放", "再生", "재생", "Reproducir", "Lire", "Abspielen") \
    X(tts_pause, "tts.pause", "Pause", "Tạm dừng", "暂停", "一時停止", "일시 정지", "Pausar", "Pause", "Pause") \
    X(tts_open_folder, "tts.open_folder", "Open output folder", "Mở thư mục đầu ra", "打开输出文件夹", "出力フォルダーを開く", "출력 폴더 열기", "Abrir carpeta de salida", "Ouvrir le dossier de sortie", "Ausgabeordner öffnen") \
    X(tts_edited, "tts.edited", "edited", "đã chỉnh sửa", "已编辑", "編集済み", "편집됨", "editado", "modifié", "bearbeitet") \
    X(tts_ready, "tts.ready", "ready", "sẵn sàng", "就绪", "準備完了", "준비됨", "listo", "prêt", "bereit") \
    X(tts_empty, "tts.empty", "empty", "trống", "空", "空", "비어 있음", "vacío", "vide", "leer") \
    X(tts_over_limit, "tts.over_limit", "Over the character limit", "Vượt quá giới hạn ký tự", "超出字符限制", "文字数上限を超えています", "글자 수 제한을 초과했습니다", "Se superó el límite de caracteres", "Limite de caractères dépassée", "Zeichenlimit überschritten") \
    X(tts_over_limit_desc, "tts.over_limit_desc", "Shorten the text, or split it into several generations.", "Rút ngắn văn bản hoặc chia thành nhiều lần tạo.", "缩短文字，或分成几次生成。", "テキストを短くするか、複数回に分けて生成してください。", "텍스트를 줄이거나 여러 번 나눠 생성하세요.", "Acorta el texto o divídelo en varias generaciones.", "Raccourcissez le texte ou divisez-le en plusieurs générations.", "Kürze den Text oder teile ihn in mehrere Generierungen auf.") \
    X(asr_title, "asr.title", "Speech to text", "Chuyển giọng nói thành văn bản", "语音转文字", "音声からテキストへ", "음성을 텍스트로", "Voz a texto", "Parole vers texte", "Sprache in Text") \
    X(asr_subtitle, "asr.subtitle", "WAV or Ogg Opus in, the transcript out. Edit it in place and diff the correction against the automatic text.", "vào WAV hoặc Ogg Opus, ra bản ghi. Chỉnh sửa tại chỗ và so sánh bản sửa với văn bản tự động.", "输入 WAV 或 Ogg Opus，输出转录文本。直接在原处编辑，并将修改与自动文本对比。", "WAV または Ogg Opus を入力し、転写文を出力。その場で編集し、修正を自動テキストと比較できます。", "WAV 또는 Ogg Opus를 입력하고 전사 결과를 출력. 바로 수정하고 수정본을 자동 텍스트와 비교하세요.", "entra WAV u Ogg Opus, sale la transcripción. Edítalo en el acto y compara la corrección con el texto automático.", "WAV ou Ogg Opus en entrée, la transcription en sortie. Modifiez-la sur place et comparez la correction au texte automatique.", "WAV oder Ogg Opus rein, das Transkript raus. Bearbeite es direkt und vergleiche die Korrektur mit dem automatischen Text.") \
    X(asr_audio, "asr.audio", "Audio", "Âm thanh", "音频", "音声", "오디오", "Audio", "Audio", "Audio") \
    X(asr_drop_title, "asr.drop_title", "Drop a WAV or Ogg Opus file here", "Thả tệp WAV hoặc Ogg Opus vào đây", "将 WAV 或 Ogg Opus 文件拖到这里", "ここに WAV または Ogg Opus ファイルをドロップ", "여기에 WAV 또는 Ogg Opus 파일을 놓으세요", "Suelta aquí un archivo WAV u Ogg Opus", "Déposez un fichier WAV ou Ogg Opus ici", "Hier eine WAV- oder Ogg-Opus-Datei ablegen") \
    X(asr_drop_desc, "asr.drop_desc", "WAV, Ogg Opus, MP3 or FLAC, mono or stereo, up to 8 MiB and 120 s", "WAV, Ogg Opus, MP3 hoặc FLAC, mono hoặc stereo, tối đa 8 MiB và 120 giây", "WAV、Ogg Opus、MP3 或 FLAC，单声道或立体声，最大 8 MiB 和 120 秒", "WAV、Ogg Opus、MP3 または FLAC、モノラルまたはステレオ、最大 8 MiB・120 秒", "WAV, Ogg Opus, MP3 또는 FLAC, 모노 또는 스테레오, 최대 8 MiB 및 120초", "WAV, Ogg Opus, MP3 o FLAC, mono o estéreo, hasta 8 MiB y 120 s", "WAV, Ogg Opus, MP3 ou FLAC, mono ou stéréo, jusqu'à 8 Mio et 120 s", "WAV, Ogg Opus, MP3 oder FLAC, Mono oder Stereo, bis 8 MiB und 120 s") \
    X(asr_browse, "asr.browse", "Browse...", "Duyệt...", "浏览...", "参照...", "찾아보기...", "Examinar...", "Parcourir...", "Durchsuchen...") \
    X(asr_transcribe, "asr.transcribe", "Transcribe", "Phiên âm", "转录", "文字起こし", "전사", "Transcribir", "Transcrire", "Transkribieren") \
    X(asr_transcript, "asr.transcript", "Transcript", "Bản ghi", "转录文本", "転写文", "전사 결과", "Transcripción", "Transcription", "Transkript") \
    X(asr_no_transcript, "asr.no_transcript", "No transcript yet", "Chưa có bản ghi", "还没有转录文本", "まだ転写文がありません", "아직 전사 결과가 없습니다", "Aún no hay transcripción", "Pas encore de transcription", "Noch kein Transkript") \
    X(asr_no_transcript_desc, "asr.no_transcript_desc", "Transcribe a file, then correct the text right here.", "Phiên âm một tệp, rồi sửa văn bản ngay tại đây.", "先转录一个文件，然后直接在这里修改文字。", "ファイルを文字起こしし、その場でテキストを修正してください。", "파일을 전사한 뒤 바로 여기서 텍스트를 수정하세요.", "Transcribe un archivo y luego corrige el texto aquí mismo.", "Transcrivez un fichier, puis corrigez le texte ici même.", "Transkribiere eine Datei und korrigiere den Text dann direkt hier.") \
    X(asr_lang_note, "asr.lang_note", "Language is detected automatically", "Ngôn ngữ được tự động phát hiện", "语言自动检测", "言語は自動的に検出されます", "언어는 자동으로 감지됩니다", "El idioma se detecta automáticamente", "La langue est détectée automatiquement", "Die Sprache wird automatisch erkannt") \
    X(asr_lang_desc, "asr.lang_desc", "On a very short clip the detector can pick the wrong language. A longer utterance is far more reliable.", "Với đoạn quá ngắn, bộ phát hiện có thể chọn sai ngôn ngữ. Một lời nói dài hơn sẽ đáng tin hơn nhiều.", "片段太短时，检测器可能选错语言。语句越长越可靠。", "非常に短いクリップでは、検出器が誤った言語を選ぶことがあります。より長い発話のほうがはるかに確実です。", "매우 짧은 클립에서는 감지기가 잘못된 언어를 고를 수 있습니다. 더 긴 발화가 훨씬 더 정확합니다.", "En un clip muy corto, el detector puede elegir el idioma equivocado. Una intervención más larga es mucho más fiable.", "Sur un extrait très court, le détecteur peut choisir la mauvaise langue. Une énonciation plus longue est bien plus fiable.", "Bei sehr kurzen Clips kann der Detektor die falsche Sprache wählen. Eine längere Äußerung ist deutlich zuverlässiger.") \
    X(asr_save, "asr.save", "Save transcript...", "Lưu bản ghi...", "保存转录文本...", "転写文を保存...", "전사 결과 저장...", "Guardar transcripción...", "Enregistrer la transcription...", "Transkript speichern...") \
    X(asr_copy, "asr.copy", "Copy transcript", "Sao chép bản ghi", "复制转录文本", "転写文をコピー", "전사 결과 복사", "Copiar transcripción", "Copier la transcription", "Transkript kopieren") \
    X(asr_revert, "asr.revert", "Revert to automatic", "Hoàn nguyên về bản tự động", "恢复为自动文本", "自動テキストに戻す", "자동 텍스트로 되돌리기", "Volver al texto automático", "Revenir au texte automatique", "Zum automatischen Text zurückkehren") \
    X(asr_correction, "asr.correction", "Correction", "Bản sửa", "修改", "修正", "수정", "Corrección", "Correction", "Korrektur") \
    X(asr_loaded, "asr.loaded", "loaded", "đã tải", "已加载", "読み込み済み", "불러옴", "cargado", "chargé", "geladen") \
    X(asr_automatic, "asr.automatic", "automatic", "tự động", "自动", "自動", "자동", "automático", "automatique", "automatisch") \
    X(mcp_title, "mcp.title", "MCP server", "Máy chủ MCP", "MCP 服务器", "MCP サーバー", "MCP 서버", "Servidor MCP", "Serveur MCP", "MCP-Server") \
    X(mcp_subtitle, "mcp.subtitle", "The same core, exposed over the Model Context Protocol on stdio. An agent calls tts_synthesize or asr_transcribe and gets the result back.", "Cùng lõi, truy cập qua Model Context Protocol trên stdio. Agent gọi tts_synthesize hoặc asr_transcribe và nhận kết quả.", "同一个核心，通过 stdio 上的 Model Context Protocol 暴露。Agent 调用 tts_synthesize 或 asr_transcribe 并取回结果。", "同じコアを、stdio 上の Model Context Protocol 経由で公開。エージェントは tts_synthesize または asr_transcribe を呼び出して結果を受け取ります。", "동일한 코어를 stdio의 Model Context Protocol을 통해 노출. 에이전트가 tts_synthesize 또는 asr_transcribe를 호출하고 결과를 받습니다.", "El mismo núcleo, expuesto mediante el Model Context Protocol por stdio. Un agente llama a tts_synthesize o asr_transcribe y recibe el resultado.", "Le même noyau, exposé via le Model Context Protocol sur stdio. Un agent appelle tts_synthesize ou asr_transcribe et récupère le résultat.", "Derselbe Kern, über das Model Context Protocol auf stdio. Ein Agent ruft tts_synthesize oder asr_transcribe auf und erhält das Ergebnis.") \
    X(mcp_tools, "mcp.tools", "Tools", "Công cụ", "工具", "ツール", "도구", "Herramientas", "Outils", "Werkzeuge") \
    X(mcp_install, "mcp.install", "Install on this machine", "Cài đặt trên máy này", "安装到此电脑", "このマシンにインストール", "이 컴퓨터에 설치", "Instalar en este equipo", "Installer sur cette machine", "Auf diesem Rechner installieren") \
    X(mcp_install_desc, "mcp.install_desc", "The binary is self-contained: C, simdjson and libcurl only. No Python, no Node, no runtime to install.", "Tệp nhị phân tự đủ: chỉ dùng C, simdjson và libcurl. Không cần Python, không cần Node, không phải cài thêm môi trường.", "二进制文件自包含：只用 C、simdjson 和 libcurl。不需要 Python、不需要 Node，也无需安装运行时。", "バイナリは自己完結です: C、simdjson、libcurl のみ。Python も Node も、インストールするランタイムも不要です。", "바이너리는 자체 포함입니다: C, simdjson, libcurl만 사용. Python도 Node도, 설치할 런타임도 필요 없습니다.", "El binario es autónomo: solo C, simdjson y libcurl. Sin Python, sin Node y sin tiempo de ejecución que instalar.", "Le binaire est autonome : C, simdjson et libcurl uniquement. Ni Python, ni Node, ni aucun runtime à installer.", "Die Binärdatei ist eigenständig: nur C, simdjson und libcurl. Kein Python, kein Node, keine Laufzeitumgebung nötig.") \
    X(mcp_client_config, "mcp.client_config", "client config", "cấu hình client", "客户端配置", "クライアント設定", "클라이언트 설정", "configuración del cliente", "configuration du client", "Client-Konfiguration") \
    X(mcp_self_test, "mcp.self_test", "Self test", "Tự kiểm tra", "自检", "セルフテスト", "자체 검사", "Autocomprobación", "Autotest", "Selbsttest") \
    X(mcp_run, "mcp.run", "Run self test", "Chạy tự kiểm tra", "运行自检", "セルフテストを実行", "자체 검사 실행", "Ejecutar autocomprobación", "Lancer l'autotest", "Selbsttest ausführen") \
    X(mcp_self_test_desc, "mcp.self_test_desc", "Runs the server binary with --selftest and shows its tools/list reply.", "Chạy tệp nhị phân máy chủ với --selftest và hiện phản hồi tools/list.", "使用 --selftest 运行服务器二进制，并显示其 tools/list 回复。", "サーバーのバイナリを --selftest で実行し、tools/list の応答を表示します。", "서버 바이너리를 --selftest로 실행하고 tools/list 응답을 표시합니다.", "Ejecuta el binario del servidor con --selftest y muestra su respuesta de tools/list.", "Exécute le binaire du serveur avec --selftest et affiche sa réponse tools/list.", "Führt die Server-Binärdatei mit --selftest aus und zeigt die tools/list-Antwort.") \
    X(settings_title, "settings.title", "Settings", "Cài đặt", "设置", "設定", "설정", "Ajustes", "Réglages", "Einstellungen") \
    X(settings_key, "settings.key", "DeepSeek API key", "Khóa API DeepSeek", "DeepSeek API 密钥", "DeepSeek API キー", "DeepSeek API 키", "Clave de API de DeepSeek", "Clé API DeepSeek", "DeepSeek-API-Schlüssel") \
    X(settings_base, "settings.base", "Base URL", "URL cơ sở", "基础 URL", "ベース URL", "기본 URL", "URL base", "URL de base", "Basis-URL") \
    X(settings_out, "settings.out", "Output folder", "Thư mục đầu ra", "输出文件夹", "出力フォルダー", "출력 폴더", "Carpeta de salida", "Dossier de sortie", "Ausgabeordner") \
    X(settings_save, "settings.save", "Save settings", "Lưu cài đặt", "保存设置", "設定を保存", "설정 저장", "Guardar ajustes", "Enregistrer les réglages", "Einstellungen speichern") \
    X(settings_reload, "settings.reload", "Reload from disk", "Tải lại từ đĩa", "从磁盘重新加载", "ディスクから再読み込み", "디스크에서 다시 불러오기", "Recargar desde el disco", "Recharger depuis le disque", "Von der Festplatte neu laden") \
    X(settings_check, "settings.check", "Check key", "Kiểm tra khóa", "检查密钥", "キーを確認", "키 확인", "Comprobar clave", "Vérifier la clé", "Schlüssel prüfen") \
    X(settings_limits, "settings.limits", "Documented limits", "Giới hạn đã ghi tài liệu", "文档记录的限额", "ドキュメント化された制限", "문서화된 제한", "Límites documentados", "Limites documentées", "Dokumentierte Grenzwerte") \
    X(settings_defaults, "settings.defaults", "Defaults and limits", "Giá trị mặc định và giới hạn", "默认值和限额", "既定値と制限", "기본값 및 제한", "Valores predeterminados y límites", "Valeurs par défaut et limites", "Standardwerte und Grenzwerte") \
    X(settings_language, "settings.language", "Interface language", "Ngôn ngữ giao diện", "界面语言", "表示言語", "인터페이스 언어", "Idioma de la interfaz", "Langue de l'interface", "Sprache der Oberfläche") \
    X(settings_theme, "settings.theme", "Theme", "Chủ đề", "主题", "テーマ", "테마", "Tema", "Thème", "Thema") \
    X(settings_key_hint, "settings.key_hint", "Stored in the OS keychain when it is available. Never printed.", "Được lưu trong kho khóa của hệ điều hành khi có. Không bao giờ hiển thị.", "储存在操作系统的钥匙串中（如有）。永不显示。", "利用可能な場合は OS のキーチェーンに保存されます。表示されることはありません。", "사용 가능한 경우 OS 키 체인에 저장됩니다. 표시되지 않습니다.", "Se guarda en el llavero del sistema cuando está disponible. Nunca se muestra.", "Stockée dans le trousseau du système lorsqu'il est disponible. Jamais affichée.", "Im OS-Schlüsselbund gespeichert, sofern verfügbar. Wird nie angezeigt.") \
    X(settings_in_force, "settings.in_force", "In force now", "Đang có hiệu lực", "当前生效", "現在適用中", "현재 적용 중", "En vigor ahora", "Actuellement appliqué", "Jetzt in Kraft") \
    X(settings_cjk_note, "settings.cjk_note", "A CJK character counts as two billed characters. A response that ends early is treated as a failure, not as short audio.", "Một ký tự CJK được tính là hai ký tự tính phí. Phản hồi kết thúc sớm được coi là lỗi, không phải âm thanh ngắn.", "一个 CJK 字符按两个计费字符计算。提前结束的响应视为失败，而不是短音频。", "CJK 文字は 2 文字として課金されます。途中で終わった応答は、短い音声ではなく失敗として扱われます。", "CJK 문자는 두 개의 청구 문자로 계산됩니다. 일찍 끝난 응답은 짧은 오디오가 아니라 실패로 간주됩니다.", "Un carácter CJK cuenta como dos caracteres facturados. Una respuesta que termina antes se trata como un fallo, no como audio corto.", "Un caractère CJK compte pour deux caractères facturés. Une réponse qui se termine tôt est traitée comme un échec, pas comme un audio court.", "Ein CJK-Zeichen zählt als zwei berechnete Zeichen. Eine vorzeitig endende Antwort gilt als Fehler, nicht als kurzes Audio.") \
    X(session_last_job, "session.last_job", "Last job", "Công việc gần nhất", "上一个任务", "最後のジョブ", "마지막 작업", "Último trabajo", "Dernière tâche", "Letzter Auftrag") \
    X(session_none, "session.none", "No job yet. The last TTS or ASR result is summarised here.", "Chưa có công việc. Kết quả TTS hoặc ASR gần nhất được tóm tắt ở đây.", "还没有任务。最近的 TTS 或 ASR 结果会汇总在这里。", "まだジョブはありません。最後の TTS または ASR の結果がここに要約されます。", "아직 작업이 없습니다. 마지막 TTS 또는 ASR 결과가 여기에 요약됩니다.", "Aún no hay trabajos. El último resultado de TTS o ASR se resume aquí.", "Aucune tâche pour l'instant. Le dernier résultat TTS ou ASR est résumé ici.", "Noch kein Auftrag. Das letzte TTS- oder ASR-Ergebnis wird hier zusammengefasst.") \
    X(session_activity, "session.activity", "Activity", "Hoạt động", "活动", "アクティビティ", "활동", "Actividad", "Activité", "Aktivität") \
    X(session_activity_none, "session.activity_none", "Nothing logged yet.", "Chưa có nhật ký nào.", "还没有日志记录。", "まだ記録はありません。", "아직 기록된 항목이 없습니다.", "Aún no hay nada registrado.", "Rien de journalisé pour l'instant.", "Noch nichts protokolliert.") \
    X(session_clear, "session.clear", "Clear history", "Xóa lịch sử", "清除历史记录", "履歴をクリア", "기록 지우기", "Borrar historial", "Effacer l'historique", "Verlauf löschen") \
    X(session_continuous_note, "session.continuous_note", "This window renders continuously so the waveform and progress stay live, which uses CPU while idle.", "Cửa sổ này vẽ liên tục để dạng sóng và tiến trình luôn cập nhật, nên tốn CPU khi rảnh.", "此窗口持续渲染，以保持波形和进度实时更新，空闲时也会占用 CPU。", "このウィンドウは波形と進捗を常に表示するため継続的に描画され、待機中にも CPU を使います。", "이 창은 파형과 진행 상황을 실시간으로 유지하기 위해 계속 렌더링되므로 유휴 상태에서도 CPU를 사용합니다.", "Esta ventana se redibuja continuamente para que la forma de onda y el progreso se mantengan en vivo, lo que consume CPU en reposo.", "Cette fenêtre se redessine en continu pour que la forme d'onde et la progression restent à jour, ce qui consomme du CPU au repos.", "Dieses Fenster rendert kontinuierlich, damit Wellenform und Fortschritt live bleiben, was im Leerlauf CPU verbraucht.") \
    X(common_edited, "common.edited", "edited", "đã sửa", "已编辑", "編集済み", "편집됨", "editado", "modifié", "bearbeitet") \
    X(common_failed, "common.failed", "failed", "thất bại", "失败", "失敗", "실패", "fallido", "échoué", "fehlgeschlagen") \
    X(common_ok, "common.ok", "ok", "ok", "正常", "OK", "정상", "ok", "ok", "ok") \
    X(common_browse, "common.browse", "Browse...", "Duyệt...", "浏览...", "参照...", "찾아보기...", "Examinar...", "Parcourir...", "Durchsuchen...") \
    X(common_edit_too_long, "common.edit_too_long", "That edit did not fit the buffer", "Mức chỉnh sửa đó không vừa bộ đệm", "该编辑超出缓冲区", "その編集はバッファに収まりませんでした", "편집 내용이 버퍼에 맞지 않습니다", "Esa edición no cabió en el búfer", "Cette modification ne tenait pas dans le tampon", "Diese Änderung passte nicht in den Puffer") \
    X(tts_fail_title, "tts.fail_title", "Synthesis failed", "Tạo giọng nói thất bại", "合成失败", "合成に失敗しました", "음성 합성 실패", "Síntesis fallida", "Échec de la synthèse", "Synthese fehlgeschlagen") \
    X(tts_clip, "tts.clip", "Generated clip", "Đoạn đã tạo", "生成的片段", "生成されたクリップ", "생성된 클립", "Clip generado", "Extrait généré", "Generiertes Clip") \
    X(tts_open_folder_fail, "tts.open_folder_fail", "Could not open the output folder", "Không mở được thư mục đầu ra", "无法打开输出文件夹", "出力フォルダーを開けませんでした", "출력 폴더를 열 수 없습니다", "No se pudo abrir la carpeta de salida", "Impossible d'ouvrir le dossier de sortie", "Ausgabeordner konnte nicht geöffnet werden") \
    X(tts_clear_cache, "tts.clear_cache", "Clear chunk cache", "Xóa bộ đệm đoạn", "清除分块缓存", "チャンクキャッシュをクリア", "청크 캐시 지우기", "Borrar caché de fragmentos", "Vider le cache des fragments", "Chunk-Cache leeren") \
    X(tts_cache_cleared, "tts.cache_cleared", "chunk cache cleared", "đã xóa bộ đệm đoạn", "分块缓存已清除", "チャンクキャッシュをクリアしました", "청크 캐시를 지웠습니다", "caché de fragmentos borrada", "cache des fragments vidé", "Chunk-Cache geleert") \
    X(tts_history_none, "tts.history_none", "Nothing yet. A generation or a transcription lands here.", "Chưa có gì. Kết quả tạo hoặc phiên âm sẽ hiện ở đây.", "还没有内容。生成或转录的结果会显示在这里。", "まだ何もありません。生成や文字起こしの結果がここに表示されます。", "아직 없습니다. 생성 또는 전사 결과가 여기에 표시됩니다.", "Aún no hay nada. Una generación o una transcripción aparecerá aquí.", "Rien pour l'instant. Une génération ou une transcription apparaît ici.", "Noch nichts. Eine Generierung oder Transkription landet hier.") \
    X(tts_placeholder, "tts.placeholder", "Write the text to read...", "Nhập văn bản cần đọc...", "输入要朗读的文字...", "読み上げるテキストを入力...", "읽을 텍스트를 입력하세요...", "Escribe el texto a leer...", "Écrivez le texte à lire...", "Zu vorlesenden Text eingeben...") \
    X(tts_col_time, "tts.col_time", "Time", "Thời gian", "时间", "時刻", "시간", "Hora", "Heure", "Zeit") \
    X(tts_col_kind, "tts.col_kind", "Kind", "Loại", "类型", "種別", "유형", "Tipo", "Type", "Art") \
    X(tts_col_detail, "tts.col_detail", "Detail", "Chi tiết", "详情", "詳細", "세부", "Detalle", "Détail", "Detail") \
    X(tts_col_size, "tts.col_size", "Size", "Kích thước", "大小", "サイズ", "크기", "Tamaño", "Taille", "Größe") \
    X(tts_col_text, "tts.col_text", "Text", "Văn bản", "文本", "テキスト", "텍스트", "Texto", "Texte", "Text") \
    X(tts_col_status, "tts.col_status", "Status", "Trạng thái", "状态", "状態", "상태", "Estado", "État", "Status") \
    X(asr_choose, "asr.choose", "Choose audio to transcribe", "Chọn tệp âm thanh để phiên âm", "选择要转录的音频", "文字起こしする音声を選択", "전사할 오디오 선택", "Elegir audio para transcribir", "Choisir l'audio à transcrire", "Zu transkribierenden Ton auswählen") \
    X(asr_fail_title, "asr.fail_title", "Transcription failed", "Phiên âm thất bại", "转录失败", "文字起こしに失敗しました", "전사 실패", "Transcripción fallida", "Échec de la transcription", "Transkription fehlgeschlagen") \
    X(asr_convert, "asr.convert", "MP3 and FLAC are converted to WAV first", "MP3 và FLAC được chuyển sang WAV trước", "MP3 和 FLAC 会先转换为 WAV", "MP3 と FLAC は先に WAV に変換됩니다", "MP3와 FLAC은 먼저 WAV로 변환됩니다", "MP3 y FLAC se convierten a WAV primero", "Les MP3 et FLAC sont d'abord convertis en WAV", "MP3 und FLAC werden zuerst in WAV konvertiert") \
    X(asr_convert_desc, "asr.convert_desc", "ASR takes WAV or Ogg Opus. Anything else is decoded locally, then uploaded as WAV.", "ASR nhận WAV hoặc Ogg Opus. Các định dạng khác được giải mã ngay trên máy rồi tải lên dưới dạng WAV.", "ASR 接受 WAV 或 Ogg Opus。其他格式先在本地解码，再以 WAV 上传。", "ASR は WAV または Ogg Opus を受け付けます。それ以外はローカルでデコードしてから WAV としてアップロードします。", "ASR은 WAV 또는 Ogg Opus를 받습니다. 다른 형식은 로컬에서 디코딩한 뒤 WAV로 업로드합니다.", "ASR acepta WAV u Ogg Opus. Cualquier otro formato se decodifica en local y se sube como WAV.", "ASR accepte WAV ou Ogg Opus. Tout autre format est décodé localement puis envoyé en WAV.", "ASR nimmt WAV oder Ogg Opus. Alles andere wird lokal dekodiert und als WAV hochgeladen.") \
    X(asr_transcribing, "asr.transcribing", "Transcribing...", "Đang phiên âm...", "正在转录...", "文字起こし中...", "전사 중...", "Transcribiendo...", "Transcription...", "Transkribiere...") \
    X(asr_corrected, "asr.corrected", "corrected", "bản sửa", "修改后", "修正済み", "수정됨", "corregido", "corrigé", "korrigiert") \
    X(asr_placeholder, "asr.placeholder", "The transcript appears here...", "Bản ghi sẽ hiện ở đây...", "转录文本会显示在这里...", "転写文がここに表示されます...", "전사 결과가 여기에 표시됩니다...", "La transcripción aparece aquí...", "La transcription apparaît ici...", "Das Transkript erscheint hier...") \
    X(asr_save_title, "asr.save_title", "Save transcript", "Lưu bản ghi", "保存转录文本", "転写文を保存", "전사 결과 저장", "Guardar transcripción", "Enregistrer la transcription", "Transkript speichern") \
    X(asr_saved, "asr.saved", "saved %s", "đã lưu %s", "已保存 %s", "保存しました: %s", "저장됨: %s", "guardado %s", "enregistré %s", "gespeichert %s") \
    X(asr_save_failed, "asr_save_failed", "cannot write %s", "không ghi được %s", "无法写入 %s", "書き込めません: %s", "쓸 수 없음: %s", "no se puede escribir %s", "impossible d'écrire %s", "kann %s nicht schreiben") \
    X(mcp_not_run, "mcp.not_run", "not run yet", "chưa chạy", "尚未运行", "まだ実行していません", "아직 실행 안 함", "aún no ejecutado", "pas encore exécuté", "noch nicht ausgeführt") \
    X(mcp_selftest_run, "mcp.selftest_run", "selftest run", "đã tự kiểm tra", "自检已完成", "セルフテスト済み", "자체 검사 실행됨", "autocomprobación ejecutada", "autotest exécuté", "Selbsttest ausgeführt") \
    X(mcp_run_again, "mcp.run_again", "Run again", "Chạy lại", "重新运行", "もう一度実行", "다시 실행", "Ejecutar de nuevo", "Relancer", "Erneut ausführen") \
    X(mcp_tool_tts, "mcp.tool_tts", "Text to audio. Writes the file, returns the audio and its metadata.", "Văn bản thành âm thanh. Ghi tệp và trả về âm thanh kèm siêu dữ liệu.", "文本转音频。写入文件并返回音频及其元数据。", "テキストを音声に変換。ファイルを書き出し、音声とメタデータを返します。", "텍스트를 오디오로 변환. 파일을 쓰고 오디오와 메타데이터를 반환합니다.", "Texto a audio. Escribe el archivo y devuelve el audio con sus metadatos.", "Texte en audio. Écrit le fichier et renvoie l'audio et ses métadonnées.", "Text zu Audio. Schreibt die Datei und liefert das Audio samt Metadaten.") \
    X(mcp_tool_asr, "mcp.tool_asr", "Audio to text. WAV or Ogg Opus, language detected.", "Âm thanh thành văn bản. WAV hoặc Ogg Opus, tự phát hiện ngôn ngữ.", "音频转文本。WAV 或 Ogg Opus，自动识别语言。", "音声をテキストに。WAV または Ogg Opus、言語は自動検出。", "오디오를 텍스트로. WAV 또는 Ogg Opus, 언어 자동 감지.", "Audio a texto. WAV u Ogg Opus, idioma detectado.", "Audio en texte. WAV ou Ogg Opus, langue détectée.", "Audio zu Text. WAV oder Ogg Opus, Sprache erkannt.") \
    X(mcp_tool_config, "mcp.tool_config", "Base URL, whether a key is set, output folder.", "URL cơ sở, khóa đã đặt hay chưa, thư mục đầu ra.", "基础 URL、是否已设置密钥、输出文件夹。", "ベース URL、キーの設定有無、出力フォルダー。", "기본 URL, 키 설정 여부, 출력 폴더.", "URL base, si hay una clave y carpeta de salida.", "URL de base, présence d'une clé, dossier de sortie.", "Basis-URL, ob ein Schlüssel gesetzt ist, Ausgabeordner.") \
    X(mcp_tool_health, "mcp.tool_health", "Liveness and version.", "Kiểm tra sống và phiên bản.", "存活状态与版本。", "生存確認とバージョン。", "생존 확인 및 버전.", "Actividad y versión.", "Vivacité et version.", "Lebendigkeit und Version.") \
    X(mcp_transport, "mcp.transport", "stdio transport, JSON-RPC 2.0. One request per line, one response per line.", "Vận chuyển stdio, JSON-RPC 2.0. Một yêu cầu mỗi dòng, một phản hồi mỗi dòng.", "stdio 传输，JSON-RPC 2.0。每行一个请求，每行一个响应。", "stdio トランスポート、JSON-RPC 2.0。1 行に 1 リクエスト、1 行に 1 レスポンス。", "stdio 전송, JSON-RPC 2.0. 한 줄에 요청 하나, 한 줄에 응답 하나.", "Transporte stdio, JSON-RPC 2.0. Una petición por línea, una respuesta por línea.", "Transport stdio, JSON-RPC 2.0. Une requête par ligne, une réponse par ligne.", "stdio-Transport, JSON-RPC 2.0. Eine Anfrage pro Zeile, eine Antwort pro Zeile.") \
    X(mcp_install_comment, "mcp.install_comment", "# copy the server next to this app into ~/.local/bin", "# sao chép máy chủ cạnh ứng dụng này vào ~/.local/bin", "# 将服务器复制到本应用旁边，放入 ~/.local/bin", "# サーバーをこのアプリの隣にコピーして ~/.local/bin に入れる", "# 서버를 이 앱 옆에 복사해 ~/.local/bin에 넣기", "# copia el servidor junto a esta app en ~/.local/bin", "# copiez le serveur à côté de cette application dans ~/.local/bin", "# Server neben diese App nach ~/.local/bin kopieren") \
    X(settings_key_source, "settings.key_source", "The key is read from $DEEPSEEK_API_KEY, then this app's own config file, then the DeepSeek provider block in kimi-code's config.", "Khóa được đọc từ $DEEPSEEK_API_KEY, rồi từ tệp cấu hình của ứng dụng, rồi từ khối DeepSeek trong cấu hình kimi-code.", "密钥依次从 $DEEPSEEK_API_KEY、本应用自己的配置文件、kimi-code 配置中的 DeepSeek 块读取。", "キーは $DEEPSEEK_API_KEY、次にこのアプリの設定ファイル、次に kimi-code の設定にある DeepSeek ブロックから読み取られます。", "$DEEPSEEK_API_KEY, 이 앱의 설정 파일, kimi-code 설정의 DeepSeek 항목 순으로 키를 읽습니다.", "La clave se lee de $DEEPSEEK_API_KEY, luego del archivo de configuración de esta app y después del bloque DeepSeek de la configuración de kimi-code.", "La clé est lue depuis $DEEPSEEK_API_KEY, puis le fichier de configuration de cette application, puis le bloc DeepSeek de la configuration kimi-code.", "Der Schlüssel wird aus $DEEPSEEK_API_KEY, dann aus der Konfigurationsdatei dieser App und dann aus dem DeepSeek-Block in kimis code gelesen.") \
    X(settings_checking, "settings.checking", "Checking...", "Đang kiểm tra...", "正在检查...", "確認中...", "확인 중...", "Comprobando...", "Vérification...", "Prüfe...") \
    X(settings_choose_out, "settings.choose_out", "Choose the output folder", "Chọn thư mục đầu ra", "选择输出文件夹", "出力フォルダーを選択", "출력 폴더 선택", "Elegir la carpeta de salida", "Choisir le dossier de sortie", "Ausgabeordner wählen") \
    X(settings_saved, "settings.saved", "saved to %s", "đã lưu vào %s", "已保存到 %s", "保存先: %s", "저장: %s", "guardado en %s", "enregistré dans %s", "gespeichert in %s") \
    X(settings_reloaded, "settings.reloaded", "reloaded", "đã tải lại", "已重新加载", "再読み込みしました", "다시 불러왔습니다", "recargado", "rechargé", "neu geladen") \
    X(settings_filter_default, "settings.filter_default", "Filter Markdown by default", "Lọc Markdown theo mặc định", "默认过滤 Markdown", "既定で Markdown を除去", "기본적으로 Markdown 필터링", "Filtrar Markdown por defecto", "Filtrer le Markdown par défaut", "Markdown standardmäßig filtern") \
    X(settings_tts_timeout, "settings.tts_timeout", "TTS timeout", "Thời gian chờ TTS", "TTS 超时", "TTS タイムアウ트", "TTS 제한 시간", "Tiempo de espera de TTS", "Délai d'attente TTS", "TTS-Timeout") \
    X(settings_asr_timeout, "settings.asr_timeout", "ASR timeout", "Thời gian chờ ASR", "ASR 超时", "ASR タイムアウト", "ASR 제한 시간", "Tiempo de espera de ASR", "Délai d'attente ASR", "ASR-Timeout") \
    X(settings_history_limit, "settings.history_limit", "History entries", "Số mục lịch sử", "历史条目数", "履歴の件数", "기록 항목 수", "Entradas del historial", "Entrées d'historique", "Verlaufseinträge") \
    X(settings_waveform_bars, "settings.waveform_bars", "Waveform bars", "Số cột dạng sóng", "波形条数", "波形のバー数", "파형 막대 수", "Barras de forma de onde", "Barres de forme d'onde", "Wellenform-Balken") \
    X(settings_lim_tts_text, "settings.lim_tts_text", "TTS TEXT", "VĂN BẢN TTS", "TTS 文本", "TTS テキスト", "TTS 텍스트", "TEXTO TTS", "TEXTE TTS", "TTS-TEXT") \
    X(settings_lim_tts_body, "settings.lim_tts_body", "TTS BODY", "KHỐI LƯỢNG TTS", "TTS 请求体", "TTS ボディ", "TTS 본문", "CUERPO TTS", "CORPS TTS", "TTS-BODY") \
    X(settings_lim_tts_length, "settings.lim_tts_length", "TTS LENGTH", "ĐỘ DÀI TTS", "TTS 时长", "TTS 長さ", "TTS 길이", "DURACIÓN TTS", "DURÉE TTS", "TTS-LÄNGE") \
    X(settings_lim_asr_file, "settings.lim_asr_file", "ASR FILE", "TỆP ASR", "ASR 文件", "ASR ファイル", "ASR 파일", "ARCHIVO ASR", "FICHIER ASR", "ASR-DATEI") \
    X(settings_lim_asr_length, "settings.lim_asr_length", "ASR LENGTH", "ĐỘ DÀI ASR", "ASR 时长", "ASR 長さ", "ASR 길이", "DURACIÓN ASR", "DURÉE ASR", "ASR-LÄNGE") \
    X(settings_lim_asr_upload, "settings.lim_asr_upload", "ASR UPLOAD", "TẢI LÊN ASR", "ASR 上传", "ASR アップロード", "ASR 업로드", "SUBIDA ASR", "ENVOI ASR", "ASR-UPLOAD")

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
