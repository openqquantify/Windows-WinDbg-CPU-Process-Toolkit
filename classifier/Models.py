import os
import re
import pandas as pd
import numpy as np
import seaborn as sns
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split, cross_val_score, StratifiedKFold
from sklearn.preprocessing import LabelEncoder
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.ensemble import RandomForestClassifier
from sklearn.svm import SVC
from sklearn.neural_network import MLPClassifier
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense, Dropout, Embedding, LSTM
from tensorflow.keras.callbacks import TensorBoard, LambdaCallback
from tensorflow.keras.wrappers.scikit_learn import KerasClassifier
from sklearn.metrics import classification_report, confusion_matrix
import umap
import plotly.express as px
import plotly.graph_objects as go
from datetime import datetime
import tensorflow as tf

# Function to read and preprocess data from classifiers directory
def preprocess_data(base_dir):
    data = []
    labels = []

    for folder in os.listdir(base_dir):
        folder_path = os.path.join(base_dir, folder)
        if os.path.isdir(folder_path):
            for file_name in os.listdir(folder_path):
                file_path = os.path.join(folder_path, file_name)
                if os.path.exists(file_path) and file_name.endswith('.txt'):
                    with open(file_path, 'r') as file:
                        content = file.read()
                        data.append(content)
                        labels.append(folder)

    return pd.DataFrame({'text': data, 'label': labels})

base_dir = 'classifiers'
df = preprocess_data(base_dir)

# Clean and preprocess the text data
def clean_text(text):
    text = re.sub(r'\s+', ' ', text)
    text = re.sub(r'[^a-zA-Z0-9\s]', '', text)
    return text

df['text'] = df['text'].apply(clean_text)

# Encode labels
label_encoder = LabelEncoder()
df['label_encoded'] = label_encoder.fit_transform(df['label'])

# Vectorize text data using TF-IDF
tfidf_vectorizer = TfidfVectorizer(max_features=10000)
X = tfidf_vectorizer.fit_transform(df['text']).toarray()
y = df['label_encoded']

# Split data into training and testing sets
X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)

# Function to create LSTM model
def create_lstm_model():
    model = Sequential([
        Embedding(input_dim=10000, output_dim=128, input_length=X_train.shape[1]),
        LSTM(128, return_sequences=True),
        LSTM(128),
        Dropout(0.5),
        Dense(128, activation='relu'),
        Dropout(0.5),
        Dense(len(label_encoder.classes_), activation='softmax')
    ])
    model.compile(optimizer='adam', loss='sparse_categorical_crossentropy', metrics=['accuracy'])
    return model

# TensorBoard setup
log_dir = "logs/fit/" + datetime.now().strftime("%Y%m%d-%H%M%S")
tensorboard_callback = TensorBoard(log_dir=log_dir, histogram_freq=1)

# Real-time plotting callback
history_plot = []

def plot_training_history(epoch, logs):
    history_plot.append(logs)
    if epoch % 1 == 0:
        clear_output(wait=True)
        plt.figure(figsize=(12, 6))
        plt.plot([h['accuracy'] for h in history_plot], label='Train Accuracy')
        plt.plot([h['val_accuracy'] for h in history_plot], label='Validation Accuracy')
        plt.title('Model Training History')
        plt.xlabel('Epoch')
        plt.ylabel('Accuracy')
        plt.legend()
        plt.show()

plot_callback = LambdaCallback(on_epoch_end=plot_training_history)

# Create and evaluate models
models = {
    'Random Forest': RandomForestClassifier(n_estimators=100),
    'SVM': SVC(kernel='linear', probability=True),
    'MLP': MLPClassifier(hidden_layer_sizes=(100,), max_iter=300),
    'LSTM': KerasClassifier(build_fn=create_lstm_model, epochs=10, batch_size=32, verbose=0)
}

results = {}
for name, model in models.items():
    if name == 'LSTM':
        skf = StratifiedKFold(n_splits=3)
        scores = cross_val_score(model, X, y, cv=skf)
    else:
        scores = cross_val_score(model, X, y, cv=5)
    results[name] = scores
    print(f'{name} Cross-Validation Accuracy: {np.mean(scores):.4f} ± {np.std(scores):.4f}')

# Train and evaluate the best model (LSTM for this example)
best_model = create_lstm_model()
history = best_model.fit(X_train, y_train, epochs=10, batch_size=32, validation_data=(X_test, y_test), callbacks=[tensorboard_callback, plot_callback])

# Evaluate the model
y_pred = np.argmax(best_model.predict(X_test), axis=-1)
print(classification_report(y_test, y_pred, target_names=label_encoder.classes_))

# Plot confusion matrix
conf_matrix = confusion_matrix(y_test, y_pred)
plt.figure(figsize=(10, 8))
sns.heatmap(conf_matrix, annot=True, fmt='d', cmap='Blues', xticklabels=label_encoder.classes_, yticklabels=label_encoder.classes_)
plt.title('Confusion Matrix')
plt.xlabel('Predicted')
plt.ylabel('Actual')
plt.show()

# Reduce dimensions using UMAP
reducer = umap.UMAP()
X_embedded = reducer.fit_transform(X)

# Plot the embeddings
fig = px.scatter(
    x=X_embedded[:, 0],
    y=X_embedded[:, 1],
    color=df['label'],
    title='UMAP Embedding of Processes',
    labels={'color': 'Process'}
)
fig.show()

# Save the model for future use
best_model.save('best_process_classification_model.h5')

# Load and test the model (for demonstration purposes)
loaded_model = tf.keras.models.load_model('best_process_classification_model.h5')
loaded_model.evaluate(X_test, y_test)

# Further detailed analysis: Feature Importance for Random Forest
rf_model = RandomForestClassifier(n_estimators=100)
rf_model.fit(X_train, y_train)
feature_importances = rf_model.feature_importances_
sorted_indices = np.argsort(feature_importances)[::-1]

plt.figure(figsize=(15, 8))
plt.title('Feature Importances')
plt.bar(range(30), feature_importances[sorted_indices[:30]], align='center')
plt.xticks(range(30), [tfidf_vectorizer.get_feature_names_out()[i] for i in sorted_indices[:30]], rotation=90)
plt.tight_layout()
plt.show()

# Additional correlation and statistical analysis
correlation_matrix = np.corrcoef(X.T)
plt.figure(figsize=(12, 10))
sns.heatmap(correlation_matrix, cmap='coolwarm', xticklabels=False, yticklabels=False)
plt.title('Feature Correlation Matrix')
plt.show()

# Analyzing specific logic patterns
def extract_logic_patterns(text):
    patterns = {
        'ModLoad': len(re.findall(r'ModLoad:', text)),
        'Break instruction exception': len(re.findall(r'Break instruction exception', text)),
        'Memory Information': len(re.findall(r'Memory Information', text)),
        'Path validation summary': len(re.findall(r'Path validation summary', text)),
        'System Information': len(re.findall(r'System Information', text)),
    }
    return patterns

logic_patterns = df['text'].apply(extract_logic_patterns)
logic_df = pd.DataFrame(logic_patterns.tolist())
logic_df['label'] = df['label']

# Correlation of logic patterns with labels
correlation_with_label = logic_df.corrwith(logic_df['label_encoded'])
plt.figure(figsize=(10, 6))
correlation_with_label.drop('label_encoded').plot(kind='bar')
plt.title('Correlation of Logic Patterns with Labels')
plt.xlabel('Logic Patterns')
plt.ylabel('Correlation')
plt.show()

# UMAP visualization of logic patterns
reducer_logic = umap.UMAP()
logic_embedded = reducer_logic.fit_transform(logic_df.drop(columns=['label', 'label_encoded']))

fig_logic = px.scatter(
    x=logic_embedded[:, 0],
    y=logic_embedded[:, 1],
    color=logic_df['label'],
    title='UMAP Embedding of Logic Patterns',
    labels={'color': 'Process'}
)
fig_logic.show()

# Train multiple models on logic patterns
X_logic = logic_df.drop(columns=['label', 'label_encoded'])
y_logic = logic_df['label_encoded']

X_train_logic, X_test_logic, y_train_logic, y_test_logic = train_test_split(X_logic, y_logic, test_size=0.2, random_state=42)

for name, model in models.items():
    if name == 'LSTM':
        skf = StratifiedKFold(n_splits=3)
        scores = cross_val_score(model, X_logic, y_logic, cv=skf)
    else:
        scores = cross_val_score(model, X_logic, y_logic, cv=5)
    results[name] = scores
    print(f'{name} on Logic Patterns Cross-Validation Accuracy: {np.mean(scores):.4f} ± {np.std(scores):.4f}')

# Train and evaluate the best model on logic patterns
best_model_logic = create_lstm_model()
history_logic = best_model_logic.fit(X_train_logic, y_train_logic, epochs=10, batch_size=32, validation_data=(X_test_logic, y_test_logic), callbacks=[tensorboard_callback, plot_callback])

# Evaluate the model on logic patterns
y_pred_logic = np.argmax(best_model_logic.predict(X_test_logic), axis=-1)
print(classification_report(y_test_logic, y_pred_logic, target_names=label_encoder.classes_))

# Plot confusion matrix for logic patterns
conf_matrix_logic = confusion_matrix(y_test_logic, y_pred_logic)
plt.figure(figsize=(10, 8))
sns.heatmap(conf_matrix_logic, annot=True, fmt='d', cmap='Blues', xticklabels=label_encoder.classes_, yticklabels=label_encoder.classes_)
plt.title('Confusion Matrix for Logic Patterns')
plt.xlabel('Predicted')
plt.ylabel('Actual')
plt.show()

# Save the model for logic patterns for future use
best_model_logic.save('best_process_logic_classification_model.h5')

# Load and test the model for logic patterns (for demonstration purposes)
loaded_model_logic = tf.keras.models.load_model('best_process_logic_classification_model.h5')
loaded_model_logic.evaluate(X_test_logic, y_test_logic)

print("Complete analysis and model training is done. Models and visualizations are ready.")
