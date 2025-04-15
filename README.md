# Projeto de Monitoramento com Zabbix, Grafana e Loki

Este projeto configura um ambiente de monitoramento utilizando Zabbix, Grafana e Loki para coletar e visualizar métricas e logs.

## Estrutura do Projeto

- **docker-compose.yml**: Configura todos os serviços necessários, incluindo MySQL, Zabbix Server, Zabbix Frontend, Grafana, Loki e Promtail.
- **loki-config.yaml**: Configuração do Loki para coleta e armazenamento de logs.
- **promtail-config.yaml**: Configuração do Promtail para envio de logs para o Loki.
- **zabbix/**: Diretório contendo scripts e configurações específicas do Zabbix.
- **zabbix-sql/**: Diretório com scripts SQL para configuração do banco de dados do Zabbix.
- **images.sql, schema.sql, data.sql**: Scripts SQL para inicialização e configuração do banco de dados.

## Ferramentas Utilizadas

- **Docker**: Para containerização dos serviços.
- **Zabbix**: Para monitoramento de métricas.
- **Grafana**: Para visualização de métricas e logs.
- **Loki**: Para agregação e consulta de logs.
- **Promtail**: Para coleta e envio de logs para o Loki.
- **MySQL**: Banco de dados para o Zabbix.

## Como Executar

1. **Configurar o Ambiente**:

   - Certifique-se de que o Docker e o Docker Compose estão instalados.

2. **Iniciar os Serviços**:

   - Execute `docker-compose up -d` para iniciar todos os serviços.

3. **Acessar o Grafana**:

   - Abra o navegador e acesse `http://localhost:3000`.
   - Adicione o Loki como fonte de dados e configure os dashboards conforme necessário.

4. **Acessar o Zabbix**:
   - Acesse `http://localhost:8080` para o frontend do Zabbix.

## Configurações Adicionais

- **Configuração do Loki e Promtail**: As configurações podem ser ajustadas diretamente no `docker-compose.yml` para simplificar a implantação.

## Contribuições

Sinta-se à vontade para contribuir com melhorias ou abrir issues para discutir problemas e sugestões.

## Utilização do MCP

O MCP (Managed Control Plane) é uma plataforma que facilita a gestão e automação de infraestrutura e serviços em nuvem. Aqui estão alguns detalhes sobre como ele pode ser utilizado no contexto do seu projeto:

### Utilização do MCP

1. **Automação de Deployments:**

   - O MCP permite automatizar o processo de deployment dos serviços, garantindo que as configurações sejam consistentes e repetíveis.

2. **Gerenciamento de Configurações:**

   - Com o MCP, você pode gerenciar configurações de forma centralizada, facilitando ajustes e atualizações nos serviços como Zabbix, Grafana e Loki.

3. **Monitoramento e Logs:**

   - Integração com ferramentas de monitoramento e logging, como Grafana e Loki, para fornecer visibilidade em tempo real sobre o desempenho e a saúde dos serviços.

4. **Escalabilidade:**

   - O MCP oferece suporte para escalar serviços de acordo com a demanda, garantindo que o sistema possa lidar com aumentos de carga sem comprometer o desempenho.

5. **Segurança:**
   - Implementa práticas de segurança para proteger dados e serviços, incluindo controle de acesso e criptografia.

### Benefícios do MCP

- **Eficiência Operacional:** Reduz o tempo e esforço necessários para gerenciar a infraestrutura.
- **Confiabilidade:** Aumenta a confiabilidade dos serviços com automação e monitoramento contínuo.
- **Flexibilidade:** Permite ajustes rápidos e fáceis nas configurações e na infraestrutura.

Esses aspectos tornam o MCP uma ferramenta valiosa para gerenciar ambientes complexos e garantir que os serviços funcionem de maneira eficiente e segura.
