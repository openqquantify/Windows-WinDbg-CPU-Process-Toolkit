import os
import re
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import dash
from dash import dcc, html
import dash_bootstrap_components as dbc
import plotly.express as px
import plotly.graph_objects as go
from sklearn.preprocessing import StandardScaler
from sklearn.decomposition import PCA
from sklearn.cluster import KMeans
from sklearn.metrics import silhouette_score

# Helper function to read files
def read_file(file_path):
    with open(file_path, 'r') as file:
        return file.read()

# Helper function to parse register states
def parse_register_states(data):
    lines = data.strip().split('\n')
    registers = [line.split('=') for line in lines if '=' in line]
    df = pd.DataFrame(registers, columns=['Register', 'Value'])
    df['Value'] = df['Value'].apply(lambda x: int(x, 16) if x.startswith('0x') else int(x))
    return df

# Helper function to parse memory information
def parse_memory_info(data):
    lines = data.strip().split('\n')
    memory = [line.split() for line in lines if 'Region Size' in line or 'Total Size' in line]
    df = pd.DataFrame(memory[1:], columns=memory[0])
    df['%ofTotal'] = df['%ofTotal'].str.replace('%', '').astype(float)
    return df

# Helper function to parse loaded modules
def parse_loaded_modules(data):
    lines = data.strip().split('\n')
    modules = [line.split() for line in lines[1:] if len(line.split()) == 4]
    df = pd.DataFrame(modules, columns=['Start', 'End', 'Module Name', 'Deferred'])
    df['Size'] = df.apply(lambda row: int(row['End'], 16) - int(row['Start'], 16), axis=1)
    return df

# Helper function to parse process uptime
def parse_process_uptime(data):
    lines = data.strip().split('\n')
    uptime_line = [line for line in lines if 'Process Uptime' in line][0]
    uptime_str = uptime_line.split(': ')[1]
    days, time_str = uptime_str.split(' days ')
    time_parts = list(map(float, re.split('[:.]', time_str)))
    uptime_seconds = int(days) * 86400 + int(time_parts[0]) * 3600 + int(time_parts[1]) * 60 + time_parts[2]
    return uptime_seconds

# Visualization for Processor Information
def visualize_processor_info(data):
    fig = go.Figure(data=[go.Table(header=dict(values=["Processor Information"]), cells=dict(values=[[data]]))])
    fig.update_layout(title="Processor Information")
    return fig

# Visualization for System Information
def visualize_system_info(data):
    fig = go.Figure(data=[go.Table(header=dict(values=["System Information"]), cells=dict(values=[[data]]))])
    fig.update_layout(title="System Information")
    return fig

# Visualization for Register States
def visualize_register_states(data):
    df = parse_register_states(data)
    fig = px.bar(df, x='Register', y='Value', title="Register States")
    return fig

# Visualization for Memory Information
def visualize_memory_info(data):
    df = parse_memory_info(data)
    fig = px.imshow(df.pivot(index='Base Address', columns='Region Size', values='%ofTotal').fillna(0).astype(float),
                    title="Memory Information Heatmap")
    return fig

# Visualization for Loaded Modules
def visualize_loaded_modules(data):
    df = parse_loaded_modules(data)
    fig = px.bar(df, x='Module Name', y='Size', title="Loaded Modules")
    fig.update_xaxes(tickangle=45)
    return fig

# Visualization for List Threads
def visualize_list_threads(data):
    lines = data.strip().split('\n')
    threads = [line.split() for line in lines[1:] if line]
    df = pd.DataFrame(threads, columns=['ID', 'Description', 'Priority', 'Priority Class', 'Affinity'])
    fig = px.histogram(df, y='Description', title="List Threads")
    return fig

# Visualization for Process Uptime
def visualize_process_uptime(data):
    uptime_seconds = parse_process_uptime(data)
    fig = px.line(x=[0, uptime_seconds], y=[0, uptime_seconds], title="Process Uptime")
    fig.update_yaxes(title='Seconds')
    fig.update_xaxes(tickangle=45)
    return fig

# Function to analyze and visualize path validation summary
def visualize_path_validation_summary(data):
    lines = data.strip().split('\n')[2:]  # Skip headers
    entries = [line.split() for line in lines]
    df = pd.DataFrame(entries, columns=['Response', 'Time (ms)', 'Location'])
    df['Time (ms)'] = df['Time (ms)'].astype(float)
    fig = px.bar(df, x='Location', y='Time (ms)', color='Response', title="Path Validation Summary")
    return fig

# Function to analyze and visualize memory layout
def visualize_memory_layout(data):
    lines = data.strip().split('\n')
    memory_entries = []
    for line in lines:
        if '---' in line or '===' in line or line.startswith(' '):
            continue
        memory_entries.append(line.split())
    df = pd.DataFrame(memory_entries, columns=['Base Address', 'Region Size'])
    fig = px.treemap(df, path=['Base Address'], values='Region Size', title="Virtual Memory Layout")
    return fig

# Perform correlation analysis
def correlation_analysis(data_frames):
    combined_df = pd.concat(data_frames, axis=1)
    corr_matrix = combined_df.corr()
    fig = px.imshow(corr_matrix, text_auto=True, title="Correlation Matrix of Debugger Output Data")
    return fig

# Perform PCA for dimensionality reduction
def pca_analysis(data_frames):
    combined_df = pd.concat(data_frames, axis=1).dropna().astype(float)
    scaler = StandardScaler()
    scaled_data = scaler.fit_transform(combined_df)
    pca = PCA(n_components=2)
    pca_result = pca.fit_transform(scaled_data)
    df_pca = pd.DataFrame(data=pca_result, columns=['PC1', 'PC2'])
    fig = px.scatter(df_pca, x='PC1', y='PC2', title="PCA Analysis of Debugger Output Data")
    return fig

# Perform KMeans clustering
def kmeans_clustering(data_frames):
    combined_df = pd.concat(data_frames, axis=1).dropna().astype(float)
    scaler = StandardScaler()
    scaled_data = scaler.fit_transform(combined_df)
    kmeans = KMeans(n_clusters=3)
    kmeans.fit(scaled_data)
    labels = kmeans.labels_
    silhouette_avg = silhouette_score(scaled_data, labels)
    df_kmeans = pd.DataFrame(data=scaled_data, columns=combined_df.columns)
    df_kmeans['Cluster'] = labels
    fig = px.scatter_matrix(df_kmeans, dimensions=combined_df.columns, color='Cluster', title=f"KMeans Clustering (Silhouette Score: {silhouette_avg:.2f})")
    return fig

# Main function to create the web application
def create_dash_app():
    base_dir = 'windbg_outputs'

    processor_info = read_file(os.path.join(base_dir, 'processor_information.txt'))
    system_info = read_file(os.path.join(base_dir, 'system_information.txt'))
    register_states = read_file(os.path.join(base_dir, 'register_states.txt'))
    memory_info = read_file(os.path.join(base_dir, 'memory_information.txt'))
    loaded_modules = read_file(os.path.join(base_dir, 'loaded_modules.txt'))
    list_threads = read_file(os.path.join(base_dir, 'list_threads.txt'))
    process_uptime = read_file(os.path.join(base_dir, 'process_uptime.txt'))
    path_validation_summary = read_file(os.path.join(base_dir, 'path_validation_summary.txt'))
    memory_layout = read_file(os.path.join(base_dir, 'memory_layout.txt'))

    data_frames = [
        parse_register_states(register_states),
        parse_memory_info(memory_info),
        parse_loaded_modules(loaded_modules)
    ]

    app = dash.Dash(__name__, external_stylesheets=[dbc.themes.BOOTSTRAP])

    app.layout = dbc.Container([
        dbc.Row([
            dbc.Col(html.H1("Debugger Output Analysis", className='text-center text-primary mb-4'), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_processor_info(processor_info)), width=6),
            dbc.Col(dcc.Graph(figure=visualize_system_info(system_info)), width=6),
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_register_states(register_states)), width=6),
            dbc.Col(dcc.Graph(figure=visualize_memory_info(memory_info)), width=6),
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_loaded_modules(loaded_modules)), width=6),
            dbc.Col(dcc.Graph(figure=visualize_list_threads(list_threads)), width=6),
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_process_uptime(process_uptime)), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_path_validation_summary(path_validation_summary)), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=visualize_memory_layout(memory_layout)), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=correlation_analysis(data_frames)), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=pca_analysis(data_frames)), width=12)
        ]),
        dbc.Row([
            dbc.Col(dcc.Graph(figure=kmeans_clustering(data_frames)), width=12)
        ]),
    ], fluid=True)

    return app

if __name__ == "__main__":
    app = create_dash_app()
    app.run_server(debug=True)
