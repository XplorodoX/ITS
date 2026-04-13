
    const schema = {
  "asyncapi": "3.0.0",
  "info": {
    "title": "AALeC Quiz API",
    "version": "1.0.0",
    "description": "MQTT-based multiplayer quiz system for AALeC hardware (ESP8266).\nThe Game Master publishes questions and state transitions.\nAALeC devices submit answers. The Beamer subscribes read-only.\n"
  },
  "servers": {
    "broker": {
      "host": "localhost:1883",
      "protocol": "mqtt",
      "description": "Eclipse Mosquitto (TCP — for firmware clients)"
    },
    "brokerWs": {
      "host": "localhost:9001",
      "protocol": "mqtts",
      "description": "Eclipse Mosquitto (WebSocket — for beamer frontend)"
    }
  },
  "channels": {
    "quiz/state": {
      "address": "quiz/state",
      "description": "Current game state. Published on every state transition.",
      "messages": {
        "stateMessage": {
          "name": "GameState",
          "summary": "Current game state with optional countdown",
          "payload": {
            "type": "object",
            "required": [
              "state",
              "question_id",
              "remaining_s"
            ],
            "properties": {
              "state": {
                "type": "string",
                "enum": [
                  "WAITING",
                  "QUESTION",
                  "VOTING",
                  "REVEAL",
                  "SCORES",
                  "ENDED"
                ],
                "example": "VOTING",
                "x-parser-schema-id": "<anonymous-schema-1>"
              },
              "question_id": {
                "type": "integer",
                "example": 3,
                "x-parser-schema-id": "<anonymous-schema-2>"
              },
              "remaining_s": {
                "type": "integer",
                "description": "Seconds remaining in current voting window (0 when not voting)",
                "example": 14,
                "x-parser-schema-id": "<anonymous-schema-3>"
              }
            },
            "example": {
              "state": "VOTING",
              "question_id": 3,
              "remaining_s": 14
            },
            "x-parser-schema-id": "GameStatePayload"
          },
          "x-parser-unique-object-id": "stateMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/state"
    },
    "quiz/question": {
      "address": "quiz/question",
      "description": "Active question with answer options. Published when a new question starts.",
      "messages": {
        "questionMessage": {
          "name": "Question",
          "summary": "A quiz question with answer options and time limit",
          "payload": {
            "type": "object",
            "required": [
              "id",
              "text",
              "options",
              "time_limit_s"
            ],
            "properties": {
              "id": {
                "type": "integer",
                "example": 3,
                "x-parser-schema-id": "<anonymous-schema-4>"
              },
              "text": {
                "type": "string",
                "example": "Was ist die Hauptstadt von Frankreich?",
                "x-parser-schema-id": "<anonymous-schema-5>"
              },
              "options": {
                "type": "object",
                "properties": {
                  "A": {
                    "type": "string",
                    "example": "Berlin",
                    "x-parser-schema-id": "<anonymous-schema-7>"
                  },
                  "B": {
                    "type": "string",
                    "example": "Paris",
                    "x-parser-schema-id": "<anonymous-schema-8>"
                  },
                  "C": {
                    "type": "string",
                    "example": "Madrid",
                    "x-parser-schema-id": "<anonymous-schema-9>"
                  },
                  "D": {
                    "type": "string",
                    "example": "Rom",
                    "x-parser-schema-id": "<anonymous-schema-10>"
                  }
                },
                "x-parser-schema-id": "<anonymous-schema-6>"
              },
              "time_limit_s": {
                "type": "integer",
                "example": 20,
                "x-parser-schema-id": "<anonymous-schema-11>"
              }
            },
            "example": {
              "id": 3,
              "text": "Was ist die Hauptstadt von Frankreich?",
              "options": {
                "A": "Berlin",
                "B": "Paris",
                "C": "Madrid",
                "D": "Rom"
              },
              "time_limit_s": 20
            },
            "x-parser-schema-id": "QuestionPayload"
          },
          "x-parser-unique-object-id": "questionMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/question"
    },
    "quiz/reveal": {
      "address": "quiz/reveal",
      "description": "Correct answer and per-option answer counts after voting closes.",
      "messages": {
        "revealMessage": {
          "name": "Reveal",
          "summary": "Correct answer key and vote counts per option",
          "payload": {
            "type": "object",
            "required": [
              "question_id",
              "correct",
              "counts"
            ],
            "properties": {
              "question_id": {
                "type": "integer",
                "example": 3,
                "x-parser-schema-id": "<anonymous-schema-12>"
              },
              "correct": {
                "type": "string",
                "enum": [
                  "A",
                  "B",
                  "C",
                  "D"
                ],
                "example": "B",
                "x-parser-schema-id": "<anonymous-schema-13>"
              },
              "counts": {
                "type": "object",
                "properties": {
                  "A": {
                    "type": "integer",
                    "example": 2,
                    "x-parser-schema-id": "<anonymous-schema-15>"
                  },
                  "B": {
                    "type": "integer",
                    "example": 11,
                    "x-parser-schema-id": "<anonymous-schema-16>"
                  },
                  "C": {
                    "type": "integer",
                    "example": 1,
                    "x-parser-schema-id": "<anonymous-schema-17>"
                  },
                  "D": {
                    "type": "integer",
                    "example": 0,
                    "x-parser-schema-id": "<anonymous-schema-18>"
                  }
                },
                "x-parser-schema-id": "<anonymous-schema-14>"
              }
            },
            "example": {
              "question_id": 3,
              "correct": "B",
              "counts": {
                "A": 2,
                "B": 11,
                "C": 1,
                "D": 0
              }
            },
            "x-parser-schema-id": "RevealPayload"
          },
          "x-parser-unique-object-id": "revealMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/reveal"
    },
    "quiz/scores": {
      "address": "quiz/scores",
      "description": "Current scoreboard, sorted by score descending.",
      "messages": {
        "scoresMessage": {
          "name": "Scores",
          "summary": "Scoreboard sorted by score descending",
          "payload": {
            "type": "object",
            "required": [
              "scores"
            ],
            "properties": {
              "scores": {
                "type": "array",
                "items": {
                  "type": "object",
                  "required": [
                    "device_id",
                    "name",
                    "score"
                  ],
                  "properties": {
                    "device_id": {
                      "type": "string",
                      "example": "aAlec-3a2f1c",
                      "x-parser-schema-id": "<anonymous-schema-21>"
                    },
                    "name": {
                      "type": "string",
                      "example": "Max",
                      "x-parser-schema-id": "<anonymous-schema-22>"
                    },
                    "score": {
                      "type": "integer",
                      "example": 1850,
                      "x-parser-schema-id": "<anonymous-schema-23>"
                    }
                  },
                  "x-parser-schema-id": "<anonymous-schema-20>"
                },
                "x-parser-schema-id": "<anonymous-schema-19>"
              }
            },
            "example": {
              "scores": [
                {
                  "device_id": "aAlec-3a2f1c",
                  "name": "Max",
                  "score": 1850
                },
                {
                  "device_id": "aAlec-07b4e9",
                  "name": "Lisa",
                  "score": 1600
                }
              ]
            },
            "x-parser-schema-id": "ScoresPayload"
          },
          "x-parser-unique-object-id": "scoresMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/scores"
    },
    "quiz/answer/{deviceId}": {
      "address": "quiz/answer/{deviceId}",
      "description": "Answer submission from a single AALeC device.",
      "parameters": {
        "deviceId": {
          "description": "Chip-ID of the AALeC device, formatted as aAlec-<hex>"
        }
      },
      "messages": {
        "answerMessage": {
          "name": "Answer",
          "summary": "Answer submission from one device",
          "payload": {
            "type": "object",
            "required": [
              "question_id",
              "answer",
              "elapsed_ms"
            ],
            "properties": {
              "question_id": {
                "type": "integer",
                "example": 3,
                "x-parser-schema-id": "<anonymous-schema-25>"
              },
              "answer": {
                "type": "string",
                "enum": [
                  "A",
                  "B",
                  "C",
                  "D"
                ],
                "example": "B",
                "x-parser-schema-id": "<anonymous-schema-26>"
              },
              "elapsed_ms": {
                "type": "integer",
                "description": "Milliseconds since the question was displayed",
                "example": 4200,
                "x-parser-schema-id": "<anonymous-schema-27>"
              }
            },
            "example": {
              "question_id": 3,
              "answer": "B",
              "elapsed_ms": 4200
            },
            "x-parser-schema-id": "AnswerPayload"
          },
          "x-parser-unique-object-id": "answerMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/answer/{deviceId}"
    },
    "quiz/connect/{deviceId}": {
      "address": "quiz/connect/{deviceId}",
      "description": "Device registration. Published by the device on boot during WAITING state.",
      "parameters": {
        "deviceId": {
          "description": "Chip-ID of the AALeC device"
        }
      },
      "messages": {
        "connectMessage": {
          "name": "Connect",
          "summary": "Device registration payload",
          "payload": {
            "type": "object",
            "required": [
              "device_id",
              "name"
            ],
            "properties": {
              "device_id": {
                "type": "string",
                "example": "aAlec-3a2f1c",
                "x-parser-schema-id": "<anonymous-schema-29>"
              },
              "name": {
                "type": "string",
                "example": "aAlec-3a2f1c",
                "x-parser-schema-id": "<anonymous-schema-30>"
              }
            },
            "example": {
              "device_id": "aAlec-3a2f1c",
              "name": "aAlec-3a2f1c"
            },
            "x-parser-schema-id": "ConnectPayload"
          },
          "x-parser-unique-object-id": "connectMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/connect/{deviceId}"
    },
    "quiz/disconnect/{deviceId}": {
      "address": "quiz/disconnect/{deviceId}",
      "description": "Last Will and Testament topic. The broker publishes this automatically\nwhen the device loses its connection.\n",
      "parameters": {
        "deviceId": {
          "description": "Chip-ID of the AALeC device"
        }
      },
      "messages": {
        "disconnectMessage": {
          "name": "Disconnect",
          "summary": "LWT payload (empty — topic presence signals disconnect)",
          "payload": {
            "type": "object",
            "x-parser-schema-id": "<anonymous-schema-32>"
          },
          "x-parser-unique-object-id": "disconnectMessage"
        }
      },
      "x-parser-unique-object-id": "quiz/disconnect/{deviceId}"
    }
  },
  "operations": {
    "publishState": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/state",
      "summary": "Game Master publishes state transitions",
      "x-parser-unique-object-id": "publishState"
    },
    "publishQuestion": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/question",
      "summary": "Game Master publishes a new question",
      "x-parser-unique-object-id": "publishQuestion"
    },
    "publishReveal": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/reveal",
      "summary": "Game Master publishes correct answer and counts",
      "x-parser-unique-object-id": "publishReveal"
    },
    "publishScores": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/scores",
      "summary": "Game Master publishes current scoreboard",
      "x-parser-unique-object-id": "publishScores"
    },
    "submitAnswer": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/answer/{deviceId}",
      "summary": "AALeC device submits its selected answer",
      "x-parser-unique-object-id": "submitAnswer"
    },
    "registerDevice": {
      "action": "send",
      "channel": "$ref:$.channels.quiz/connect/{deviceId}",
      "summary": "AALeC device registers itself on boot",
      "x-parser-unique-object-id": "registerDevice"
    },
    "receiveAnswer": {
      "action": "receive",
      "channel": "$ref:$.channels.quiz/answer/{deviceId}",
      "summary": "Game Master collects answers from all devices",
      "x-parser-unique-object-id": "receiveAnswer"
    },
    "receiveConnect": {
      "action": "receive",
      "channel": "$ref:$.channels.quiz/connect/{deviceId}",
      "summary": "Game Master registers new players",
      "x-parser-unique-object-id": "receiveConnect"
    },
    "receiveDisconnect": {
      "action": "receive",
      "channel": "$ref:$.channels.quiz/disconnect/{deviceId}",
      "summary": "Game Master handles device disconnects (LWT)",
      "x-parser-unique-object-id": "receiveDisconnect"
    }
  },
  "components": {
    "messages": {
      "GameState": "$ref:$.channels.quiz/state.messages.stateMessage",
      "Question": "$ref:$.channels.quiz/question.messages.questionMessage",
      "Reveal": "$ref:$.channels.quiz/reveal.messages.revealMessage",
      "Scores": "$ref:$.channels.quiz/scores.messages.scoresMessage",
      "Answer": "$ref:$.channels.quiz/answer/{deviceId}.messages.answerMessage",
      "Connect": "$ref:$.channels.quiz/connect/{deviceId}.messages.connectMessage",
      "Disconnect": "$ref:$.channels.quiz/disconnect/{deviceId}.messages.disconnectMessage"
    },
    "schemas": {
      "GameStatePayload": "$ref:$.channels.quiz/state.messages.stateMessage.payload",
      "QuestionPayload": "$ref:$.channels.quiz/question.messages.questionMessage.payload",
      "RevealPayload": "$ref:$.channels.quiz/reveal.messages.revealMessage.payload",
      "ScoresPayload": "$ref:$.channels.quiz/scores.messages.scoresMessage.payload",
      "AnswerPayload": "$ref:$.channels.quiz/answer/{deviceId}.messages.answerMessage.payload",
      "ConnectPayload": "$ref:$.channels.quiz/connect/{deviceId}.messages.connectMessage.payload"
    }
  },
  "x-parser-spec-parsed": true,
  "x-parser-api-version": 3,
  "x-parser-spec-stringified": true
};
    const config = {"show":{"sidebar":true},"sidebar":{"showOperations":"byDefault"}};
    const appRoot = document.getElementById('root');
    AsyncApiStandalone.render(
        { schema, config, }, appRoot
    );
  